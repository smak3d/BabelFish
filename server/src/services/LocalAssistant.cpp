#include "LocalAssistant.h"

#include <llama.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iterator>

LocalAssistant::LocalAssistant(std::string modelPath, int device, int gpuLayers)
    : modelPath(std::move(modelPath)), device(device), gpuLayers(gpuLayers)
{
    llama_backend_init();
    llama_model_params modelParams = llama_model_default_params();

    // На GPU уезжает ровно n_gpu_layers слоёв; остальные считаются в RAM.
    // split_mode NONE — модель целиком на main_gpu, а не размазана по всем
    // картам: так каждая модель привязана к своему устройству явно.
    modelParams.n_gpu_layers = (device >= 0) ? gpuLayers : 0;
    modelParams.split_mode   = LLAMA_SPLIT_MODE_NONE;
    modelParams.main_gpu     = (device >= 0) ? device : 0;

    model = llama_model_load_from_file(
        this->modelPath.c_str(),
        modelParams
    );


    if (!model)
    {
        throw std::runtime_error(
            "Failed to load model: " + this->modelPath);
    }

    llama_context_params contextParams =
        llama_context_default_params();

    contextParams.n_ctx = 4096;

    context = llama_init_from_model(
        model,
        contextParams
    );

    if (!context)
    {
        llama_model_free(model);
        model = nullptr;

        throw std::runtime_error("Failed to create context");
    }
}

LocalAssistant::~LocalAssistant()
{
    if (context)
    {
        llama_free(context);
        context = nullptr;
    }

    if (model)
    {
        llama_model_free(model);
        model = nullptr;
    }

    llama_backend_free();
}

const std::string& LocalAssistant::getPath() const
{
    return modelPath;
}

std::string LocalAssistant::prompt(const std::string& systemPrompt,
                                   const std::string& text)
{
    if (!model || !context)
    {
        throw std::runtime_error(
            "Assistant is not initialized"
        );
    }


const llama_chat_message messages[] = {
    { "system", systemPrompt.c_str() },
    { "user",   text.c_str()         },
};

const char* chatTemplate = llama_model_chat_template(model, nullptr);
if (!chatTemplate)
{
    throw std::runtime_error("The model does not contain a chat template");
}

const int32_t promptLength = llama_chat_apply_template(
    chatTemplate,
    messages,
    std::size(messages),
    true,
    nullptr,
    0);

if (promptLength < 0)
{
    throw std::runtime_error("Failed to calculate the chat prompt size");
}

std::vector<char> promptBuffer(promptLength + 1);

const int32_t formattedLength = llama_chat_apply_template(
    chatTemplate,
    messages,
    std::size(messages),
    true,
    promptBuffer.data(),
    static_cast<int32_t>(promptBuffer.size()));

if (formattedLength < 0)
{
    throw std::runtime_error("Failed to apply the chat template");
}

const std::string formattedPrompt(
    promptBuffer.data(),
    formattedLength);

    const llama_vocab* vocab =
        llama_model_get_vocab(model);

    // Every request is independent.
    // Clear the KV cache from the previous request.
    llama_memory_clear(
        llama_get_memory(context),
        true
    );

    int tokenCount = -llama_tokenize(
        vocab,
        formattedPrompt.c_str(),
        formattedPrompt.size(),
        nullptr,
        0,
        true,
        true
    );

    if (tokenCount <= 0)
    {
        throw std::runtime_error(
            "Failed to tokenize input"
        );
    }

    std::vector<llama_token> tokens(tokenCount);

    int result = llama_tokenize(
        vocab,
        formattedPrompt.c_str(),
        formattedPrompt.size(),
        tokens.data(),
        tokens.size(),
        true,
        true
    );

    if (result < 0)
    {
        throw std::runtime_error(
            "Tokenization failed"
        );
    }

    tokenCount = result;

    if (tokenCount >= static_cast<int>(
            llama_n_ctx(context)))
    {
        throw std::runtime_error(
            "Input exceeds context size"
        );
    }

    
    llama_batch batch =
        llama_batch_init(
            tokenCount,
            0,
            1
        );

    for (int i = 0; i < tokenCount; ++i)
    {
        batch.token[i] = tokens[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] =
            (i == tokenCount - 1);
    }

    batch.n_tokens = tokenCount;

    result = llama_decode(
        context,
        batch
    );

    if (result != 0)
    {
        llama_batch_free(batch);

        throw std::runtime_error(
            "llama_decode failed for prompt"
        );
    }

    llama_sampler_chain_params samplerParams =
        llama_sampler_chain_default_params();

    llama_sampler* sampler =
        llama_sampler_chain_init(
            samplerParams
        );

    if (!sampler)
    {
        llama_batch_free(batch);

        throw std::runtime_error(
            "Failed to initialize sampler"
        );
    }

    llama_sampler_chain_add(
        sampler,
        llama_sampler_init_temp(0.7f)
    );

    llama_sampler_chain_add(
        sampler,
        llama_sampler_init_top_p(0.9f, 1)
    );

    llama_sampler_chain_add(
        sampler,
        llama_sampler_init_dist(1234)
    );

    std::string outputText;

    constexpr int maxOutputTokens = 128;

    for (int i = 0; i < maxOutputTokens; ++i)
    {
        llama_token token =
            llama_sampler_sample(
                sampler,
                context,
                -1
            );

        if (llama_vocab_is_eog(vocab, token))
        {
            break;
        }

        char piece[256];

        int length = llama_token_to_piece(
            vocab,
            token,
            piece,
            sizeof(piece),
            0,
            true
        );

        if (length < 0)
        {
            llama_sampler_free(sampler);
            llama_batch_free(batch);

            throw std::runtime_error(
                "Failed to convert token to text"
            );
        }

        if (length > 0)
        {
            outputText.append(
                piece,
                length
            );
        }

        llama_sampler_accept(
            sampler,
            token
        );

        /*
         * Reuse the same batch for one generated token.
         */
        batch.token[0] = token;
        batch.pos[0] = tokenCount + i;
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;
        batch.logits[0] = true;
        batch.n_tokens = 1;

        result = llama_decode(
            context,
            batch
        );

        if (result != 0)
        {
            llama_sampler_free(sampler);
            llama_batch_free(batch);

            throw std::runtime_error(
                "llama_decode failed during generation"
            );
        }
    }

    llama_sampler_free(sampler);
    llama_batch_free(batch);

    return outputText;
}

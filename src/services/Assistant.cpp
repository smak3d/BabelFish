#include "Assistant.h"

#include <llama.h>

#include <iostream>
#include <string>
#include <vector>

namespace
{
    llama_model* model = nullptr;
    llama_context* context = nullptr;

    const char* modelPath= "C:/models/Qwen3-4B-Q6_K.gguf";
}

Assistant::Assistant()
{
    llama_backend_init();
    llama_model_params modelParams = llama_model_default_params();

    model = llama_load_model_from_file(modelPath, modelParams);


    if (!model)
    {
        throw std::runtime_error("Failed to load model");
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
        llama_free_model(model);
        model = nullptr;

        throw std::runtime_error("Failed to create context");
    }
}

Assistant::~Assistant()
{
    if (context)
    {
        llama_free(context);
        context = nullptr;
    }

    if (model)
    {
        llama_free_model(model);
        model = nullptr;
    }
    
    llama_backend_free();
}

std::string Assistant::prompt(const std::string& text)
{
     if (!model || !context)
    {
        throw std::runtime_error("Assistant is not initialized");
    }

    const llama_vocab* vocab =
        llama_model_get_vocab(model);

    std::vector<llama_token> tokens(
        text.size() + 32
    );

    int tokenCount = llama_tokenize(
        vocab,
        text.c_str(),
        text.size(),
        tokens.data(),
        tokens.size(),
        true,
        true
    );

    if (tokenCount < 0)
    {
        tokens.resize(-tokenCount);

        tokenCount = llama_tokenize(
            vocab,
            text.c_str(),
            text.size(),
            tokens.data(),
            tokens.size(),
            true,
            true
        );
    }

    if (tokenCount < 0)
    {
        throw std::runtime_error("Tokenization failed");
    }

    tokens.resize(tokenCount);

    llama_batch batch =
        llama_batch_init(
            512,
            0,
            1
        );

    for (int i = 0; i < tokenCount; ++i)
    {
        batch.token[i] = tokens[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == tokenCount - 1);
    }

    batch.n_tokens = tokenCount;

    int result = llama_decode(
        context,
        batch
    );

    if (result != 0)
    {
        llama_batch_free(batch);
        throw std::runtime_error("llama_decode failed");
    }

    llama_sampler_chain_params samplerParams =
        llama_sampler_chain_default_params();

    llama_sampler* sampler =
        llama_sampler_chain_init(samplerParams);

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

    for (int i = 0; i < 128; ++i)
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

        if (length > 0)
        {
            outputText.append(piece, length);
        }

        llama_sampler_accept(
            sampler,
            token
        );

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
            break;
        }
    }

    llama_sampler_free(sampler);
    llama_batch_free(batch);

    return outputText;
}
// П.8 (связка): запрос -> маршрутизатор -> исполнитель -> LLM.
// Исполнители получают PromptFn-заглушку: без моделей и без сети
// проверяется, КАКОЙ запрос уходит в LLM (инструкции + сырой текст)
// и что ответ доходит до вызывающего.

#include "executors/Executors.h"

#include "Router.h"

#include <cstdio>
#include <exception>
#include <string>

namespace
{
    int checksRun    = 0;
    int checksFailed = 0;

    void check(bool condition, const char* expression, int line)
    {
        ++checksRun;
        if (!condition)
        {
            std::printf("FAIL line %d: %s\n", line, expression);
            ++checksFailed;
        }
    }

    // Заглушка LLM: записывает, что ушло, и возвращает заранее
    // известный ответ — проверяем факт отправки, а не генерацию.
    struct FakeLlm
    {
        std::string system;
        std::string user;
        std::string reply = "llm-reply";

        PromptFn fn()
        {
            return [this](const std::string& systemPrompt,
                          const std::string& text)
            {
                system = systemPrompt;
                user   = text;
                return reply;
            };
        }
    };
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// Переводчик: классификатор -> выбор -> свой конкретный запрос в LLM
// ---------------------------------------------------------------------------
static void testTranslationChain()
{
    FakeLlm llm;
    TranslatorExecutor executor(llm.fn());

    const std::string raw = "переведи на английский";
    const auto [intent, params] = Router::classify(raw);

    CHECK(Router::executorFor(intent, params) == Router::Executor::Translator);

    const auto answer = executor.execute(raw, params);

    CHECK(llm.user == raw); // сырой текст уходит как есть
    CHECK(llm.system.find(params.targetLang) != std::string::npos);
    CHECK(llm.system.find("переводчик") != std::string::npos);
    CHECK(answer == llm.reply);
}

// ---------------------------------------------------------------------------
// Дефолт: вопрос -> свои инструкции ассистента, текст как есть
// ---------------------------------------------------------------------------
static void testAnswerChain()
{
    FakeLlm llm;
    LocalAssistantExecutor executor(llm.fn());

    const std::string raw = "Чем отличается REQ от REP?";
    const auto [intent, params] = Router::classify(raw);

    CHECK(Router::executorFor(intent, params)
          == Router::Executor::LocalAssistant);

    const auto answer = executor.execute(raw, params);

    CHECK(llm.user == raw);
    CHECK(llm.system.find("голосовой ассистент") != std::string::npos);
    CHECK(answer == llm.reply);
}

// ---------------------------------------------------------------------------
// Роль определяет инструкции, а не транспорт: два исполнителя на одной
// и той же LLM отправляют разные запросы.
// ---------------------------------------------------------------------------
static void testInstructionsDiffer()
{
    FakeLlm llm;
    LocalAssistantExecutor answer(llm.fn());
    answer.execute("вопрос", {});

    const std::string answerSystem = llm.system;

    TranslatorExecutor translator(llm.fn());
    translator.execute("текст", { "fr" });

    CHECK(llm.system != answerSystem);
    CHECK(llm.system.find("fr") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Удалённый исполнитель без тела (п.8): ошибка, а не тихий фолбэк
// ---------------------------------------------------------------------------
static void testRemoteUnavailable()
{
    RemoteTranslatorExecutor executor;

    bool threw  = false;
    std::string message;

    try
    {
        executor.execute("переведи на французский", { "fr" });
    }
    catch (const std::exception& exception)
    {
        threw  = true;
        message = exception.what();
    }

    CHECK(threw);
    CHECK(message.find("not available") != std::string::npos);
}

int main()
{
    testTranslationChain();
    testAnswerChain();
    testInstructionsDiffer();
    testRemoteUnavailable();

    std::printf("executor chain: %d checks, %d failed\n",
                checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}

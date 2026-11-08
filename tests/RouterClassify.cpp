// П.8 (роутер): classify() и executorFor() — чистые функции, таблица
// фраз -> ожидаемое намерение/целевой язык/исполнитель. Без моделей и
// без сети: Router.cpp компилируется напрямую — тот же приём, что у
// TransportEcho с NetworkServer.

#include "Router.h"

#include <cstdio>
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

    struct Sample
    {
        const char*      text;
        Router::Intent   intent;
        const char*      targetLang;
        Router::Executor executor;
    };

    const Sample samples[] = {
        // Перевод на локальной LLM — отдельный исполнитель со своими
        // инструкциями (см. ExecutorChain).
        { "переведи на английский",   Router::Intent::Translator,     "en", Router::Executor::Translator },
        { "переведи на русский",      Router::Intent::Translator,     "ru", Router::Executor::Translator },
        // Остальные языки — удалённый.
        { "переведи на французский",  Router::Intent::Translator,     "fr", Router::Executor::RemoteTranslator },
        { "Переведи на немецкий",     Router::Intent::Translator,     "de", Router::Executor::RemoteTranslator },
        // Регистр («П» в начале фразы) и падеж («на французском»).
        { "ПЕРЕВЕДИ НА ФРАНЦУЗСКИЙ",  Router::Intent::Translator,     "fr", Router::Executor::RemoteTranslator },
        { "переведи на французском",  Router::Intent::Translator,     "fr", Router::Executor::RemoteTranslator },
        // Словоформы глагола: «перевести», «перевод».
        { "перевести на японский",    Router::Intent::Translator,     "ja", Router::Executor::RemoteTranslator },
        { "нужен перевод на китайский", Router::Intent::Translator,  "zh", Router::Executor::RemoteTranslator },
        // Язык не извлечён — решения нет, фолбэк в LocalAssistant.
        { "просто переведи",          Router::Intent::Translator,     "",   Router::Executor::None },
        // Дефолт: вопрос идёт локальному ассистенту.
        { "Чем отличается REQ от REP?", Router::Intent::LocalAssistant, "", Router::Executor::LocalAssistant },
        { "",                          Router::Intent::LocalAssistant, "", Router::Executor::LocalAssistant },
    };
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// Таблица фраз -> намерение, целевой язык, исполнитель
// ---------------------------------------------------------------------------
static void testClassifyTable()
{
    for (const auto& sample : samples)
    {
        const auto [intent, params] = Router::classify(sample.text);

        const int failedBefore = checksFailed;
        CHECK(intent == sample.intent);
        CHECK(params.targetLang == sample.targetLang);
        CHECK(Router::executorFor(intent, params) == sample.executor);

        if (checksFailed != failedBefore)
            std::printf("  sample: \"%s\"\n", sample.text);
    }
}

// ---------------------------------------------------------------------------
// executorFor напрямую: намерение без языка не уходит в удалённый,
// локальное намерение важнее параметров.
// ---------------------------------------------------------------------------
static void testExecutorFor()
{
    CHECK(Router::executorFor(Router::Intent::LocalAssistant, { "fr" })
          == Router::Executor::LocalAssistant);
    CHECK(Router::executorFor(Router::Intent::Translator, { "" })
          == Router::Executor::None);
    CHECK(Router::executorFor(Router::Intent::Translator, { "en" })
          == Router::Executor::Translator);
    CHECK(Router::executorFor(Router::Intent::Translator, { "de" })
          == Router::Executor::RemoteTranslator);
}

int main()
{
    testClassifyTable();
    testExecutorFor();

    std::printf("router classify: %d checks, %d failed\n",
                checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}

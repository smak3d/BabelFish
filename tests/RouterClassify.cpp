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
        Router::ExecutorType executor;
    };

    const Sample samples[] = {
        // Перевод на локальной LLM — отдельный исполнитель со своими
        // инструкциями (см. ExecutorChain).
        { "переведи на английский",   Router::Intent::Translator,     "en", Router::ExecutorType::Translator },
        { "переведи на русский",      Router::Intent::Translator,     "ru", Router::ExecutorType::Translator },
        // Остальные языки — удалённый.
        { "переведи на французский",  Router::Intent::Translator,     "fr", Router::ExecutorType::RemoteTranslator },
        { "Переведи на немецкий",     Router::Intent::Translator,     "de", Router::ExecutorType::RemoteTranslator },
        // Регистр («П» в начале фразы) и падеж («на французском»).
        { "ПЕРЕВЕДИ НА ФРАНЦУЗСКИЙ",  Router::Intent::Translator,     "fr", Router::ExecutorType::RemoteTranslator },
        { "переведи на французском",  Router::Intent::Translator,     "fr", Router::ExecutorType::RemoteTranslator },
        // Словоформы глагола: «перевести», «перевод».
        { "перевести на японский",    Router::Intent::Translator,     "ja", Router::ExecutorType::RemoteTranslator },
        { "нужен перевод на китайский", Router::Intent::Translator,  "zh", Router::ExecutorType::RemoteTranslator },
        // Два языка в запросе: цель — после предлога «на», а не первое
        // название в строке и не порядок словаря.
        { "переведи с английского на французский", Router::Intent::Translator, "fr", Router::ExecutorType::RemoteTranslator },
        { "переведи с русского на немецкий",       Router::Intent::Translator, "de", Router::ExecutorType::RemoteTranslator },
        { "переведи с французского на русский",    Router::Intent::Translator, "ru", Router::ExecutorType::Translator },
        { "переведи на французский с английского", Router::Intent::Translator, "fr", Router::ExecutorType::RemoteTranslator },
        // Известные ограничения правил — осознанные, не баги: отрицания
        // не разбираем. Без языка вреда нет (None -> фолбэк), с языком —
        // ложный RemoteTranslator. Upgrade — LLM-классификатор (Router.h).
        { "не переводи, а объясни",     Router::Intent::Translator,     "",  Router::ExecutorType::None },
        { "не переводи на французский", Router::Intent::Translator,     "fr", Router::ExecutorType::RemoteTranslator },
        // Язык не извлечён — решения нет, фолбэк в LocalAssistant.
        { "просто переведи",          Router::Intent::Translator,     "",   Router::ExecutorType::None },
        // Дефолт: вопрос идёт локальному ассистенту.
        { "Чем отличается REQ от REP?", Router::Intent::LocalAssistant, "", Router::ExecutorType::LocalAssistant },
        { "",                          Router::Intent::LocalAssistant, "", Router::ExecutorType::LocalAssistant },
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
          == Router::ExecutorType::LocalAssistant);
    CHECK(Router::executorFor(Router::Intent::Translator, { "" })
          == Router::ExecutorType::None);
    CHECK(Router::executorFor(Router::Intent::Translator, { "en" })
          == Router::ExecutorType::Translator);
    CHECK(Router::executorFor(Router::Intent::Translator, { "de" })
          == Router::ExecutorType::RemoteTranslator);
}

// ---------------------------------------------------------------------------
// languageName: ISO-код -> название для инструкции исполнителя;
// неизвестный код не роняет, возвращается как есть.
// ---------------------------------------------------------------------------
static void testLanguageName()
{
    CHECK(Router::languageName("fr") == "французский");
    CHECK(Router::languageName("en") == "английский");
    CHECK(Router::languageName("ru") == "русский");
    CHECK(Router::languageName("xx") == "xx");
}

int main()
{
    testClassifyTable();
    testExecutorFor();
    testLanguageName();

    std::printf("router classify: %d checks, %d failed\n",
                checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}

#include "Router.h"
#include "RouterRules.h"

#include <utility>

namespace
{

    // Строчные буквы: ASCII A-Z. Кириллицу std::tolower с локалью C не
    // трогает, а «Переведи» в начале фразы — самый частый случай, поэтому
    // UTF-8 руками: А..П = D0 90..9F -> D0 B0..BF, Р..Я = D0 A0..AF ->
    // D1 80..8F, Ё = D0 81 -> D1 91; строчные не трогаем.
    std::string lowercase(std::string text)
    {
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const auto byte = static_cast<unsigned char>(text[index]);

            if (byte >= 'A' && byte <= 'Z')
            {
                text[index] = static_cast<char>(byte + 32);
            }
            // 0xD0 — ведущий байт кириллицы U+0400..U+043F; index+1 —
            // обрезанный UTF-8 (ведущий байт в конце) не читаем за границу
            else if (byte == 0xD0 && index + 1 < text.size())
            {
                const auto next = static_cast<unsigned char>(text[index + 1]);

                if (next >= 0x90 && next <= 0x9F) // А..П
                {
                    text[index + 1] = static_cast<char>(next + 0x20);
                }
                else if (next >= 0xA0 && next <= 0xAF) // Р..Я
                {
                    text[index]     = static_cast<char>(0xD1);
                    text[index + 1] = static_cast<char>(next - 0x20);
                }
                else if (next == 0x81) // Ё
                {
                    text[index]     = static_cast<char>(0xD1);
                    text[index + 1] = static_cast<char>(0x91);
                }
            }
        }

        return text;
    }
}

std::pair<Router::Intent, Router::Params>
Router::classify(const std::string& text)
{
    const std::string words = lowercase(text);

    Params params;
    for (const auto& [stem, code] : RouterRules::languages)
    {
        if (words.find(stem) != std::string::npos)
        {
            params.targetLang = code;
            break; // два языка в одном тексте — берём первый: правило наивное
        }
    }

    bool wantsTranslation = false;
    for (const char* keyword : RouterRules::keywords)
    {
        if (words.find(keyword) != std::string::npos)
        {
            wantsTranslation = true;
            break;
        }
    }

    return { wantsTranslation ? Intent::Translator : Intent::LocalAssistant,
             params };
}

Router::Executor
Router::executorFor(Intent intent, const Params& params)
{
    if (intent == Intent::LocalAssistant)
        return Executor::LocalAssistant;

    // Язык не извлечён — решения нет: фолбэк, роутер не ломает запрос.
    if (params.targetLang.empty())
        return Executor::None;

    // Правило наивное: переводчик ru<->en на локальной LLM, остальное —
    // удалённый. Capability-реестр не строим: он появится сам, когда
    // исполнителей станет много и начнут отваливаться.
    if (params.targetLang == "ru" || params.targetLang == "en")
        return Executor::Translator;

    return Executor::RemoteTranslator;
}

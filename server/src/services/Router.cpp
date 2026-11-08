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

    // Первый язык в строке начиная с позиции from — по позиции в тексте,
    // а не по порядку словаря: «на французский с английского» -> fr.
    std::string firstLanguageIn(const std::string& words, std::size_t from)
    {
        std::string found;
        std::size_t foundAt = std::string::npos;

        for (const auto& language : RouterRules::languages)
        {
            const auto at = words.find(language.stem, from);
            if (at != std::string::npos && at < foundAt)
            {
                foundAt = at;
                found   = language.code;
            }
        }

        return found;
    }

    // Позиция после предлога «на» — там стоит целевой язык («переведи
    // с английского на французский»). 0 — конструкции нет, тогда поиск
    // идёт по всей строке. Правило наивное: отрицания и «вместо» не
    // разбираем (см. известные ограничения в RouterClassify).
    std::size_t targetAfterNa(const std::string& words)
    {
        for (std::size_t at = words.find("на ");
             at != std::string::npos;
             at = words.find("на ", at + 1))
        {
            if (at == 0 || words[at - 1] == ' ') // отдельное слово, не «сона»
                return at + 3;                   // после «на «
        }

        return 0;
    }
}

std::pair<Router::Intent, Router::Params>
Router::classify(const std::string& text)
{
    const std::string words = lowercase(text);

    Params params;

    // Целевой язык — после «на», иначе первое название в строке, иначе
    // пусто -> executorFor вернёт None (фолбэк, запрос не ломается).
    const std::size_t region = targetAfterNa(words);
    std::string target = firstLanguageIn(words, region);
    if (target.empty() && region != 0)
        target = firstLanguageIn(words, 0); // «на» есть, языка после него нет

    params.targetLang = std::move(target);

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

Router::ExecutorType
Router::executorFor(Intent intent, const Params& params)
{
    if (intent == Intent::LocalAssistant)
        return ExecutorType::LocalAssistant;

    // Язык не извлечён — решения нет: фолбэк, роутер не ломает запрос.
    if (params.targetLang.empty())
        return ExecutorType::None;

    // Правило наивное: переводчик ru<->en на локальной LLM, остальное —
    // удалённый. Capability-реестр не строим: он появится сам, когда
    // исполнителей станет много и начнут отваливаться.
    if (params.targetLang == "ru" || params.targetLang == "en")
        return ExecutorType::Translator;

    return ExecutorType::RemoteTranslator;
}

std::string Router::languageName(const std::string& code)
{
    for (const auto& language : RouterRules::languages)
        if (code == language.code)
            return language.name;

    return code; // неизвестный код — оставляем как есть
}

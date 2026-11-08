#include "executors/Executors.h"

#include <stdexcept>
#include <utility>

LocalAssistantExecutor::LocalAssistantExecutor(PromptFn prompt)
    : prompt(std::move(prompt))
{
}

std::string LocalAssistantExecutor::execute(const std::string& text,
                                            const Router::Params&)
{
    // Дефолтные инструкции жили в LocalAssistant — переезжают сюда:
    // как отвечать, решает исполнитель, а не транспорт.
    return prompt(
        "Ты полезный голосовой ассистент. "
        "Отвечай кратко, точно и по существу на русском языке.",
        text);
}

TranslatorExecutor::TranslatorExecutor(PromptFn prompt)
    : prompt(std::move(prompt))
{
}

std::string TranslatorExecutor::execute(const std::string& text,
                                        const Router::Params& params)
{
    // Нормализация входа — наша: сырой текст роутера превращаем в
    // конкретный запрос. params.targetLang приходит из словаря
    // Router (RouterRules.h), не извне — подстановка безопасна.
    return prompt(
        "Ты профессиональный переводчик. "
        "Переведи пользовательский текст на язык " + params.targetLang + ". "
        "Отвечай только переводом, без пояснений и кавычек.",
        text);
}

std::string RemoteTranslatorExecutor::execute(const std::string&,
                                              const Router::Params&)
{
    // shortcut: у NetAssistant нет тела (п.8) — удалённый исполнитель
    // недоступен, и это ошибка, а не тихий фолбэк в локальный (молчаливо
    // неправильный перевод хуже ошибки).
    // Upgrade: тело NetAssistant с явным таймаутом (п.8) — сюда же,
    // execute пойдёт в сеть, статус ошибки у вызывающего не меняется.
    throw std::runtime_error("remote translator is not available yet");
}

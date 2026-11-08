#pragma once

#include "executors/Executor.h"

#include <string>

// Реальные исполнители. Инструкции (system prompt) лежат здесь же —
// это владение исполнителя, а не транспорта. Транспорт один на всех:
// PromptFn (локальная LLM сегодня, удалённый — когда появится тело
// NetAssistant).

// Ответить на вопрос — дефолт, как раньше до п.8.
class LocalAssistantExecutor : public Executor
{
    public:
        explicit LocalAssistantExecutor(PromptFn prompt);

        std::string execute(const std::string& text,
                            const Router::Params& params) override;

    private:
        PromptFn prompt;
};

// Перевод: свой конкретный запрос с целевым языком из params.
class TranslatorExecutor : public Executor
{
    public:
        explicit TranslatorExecutor(PromptFn prompt);

        std::string execute(const std::string& text,
                            const Router::Params& params) override;

    private:
        PromptFn prompt;
};

// Заглушка до тела NetAssistant (п.8): удалённый исполнитель
// недоступен — ошибка, а не тихий фолбэк в локальный.
class RemoteTranslatorExecutor : public Executor
{
    public:
        std::string execute(const std::string& text,
                            const Router::Params& params) override;
};

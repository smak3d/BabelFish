#pragma once

#include "Router.h"

#include <functional>
#include <string>

// П.8, связка «запрос -> маршрутизатор -> исполнитель -> LLM».
//
// Исполнитель владеет СВОИМИ инструкциями (как отвечать и что делать)
// и шлёт их в LLM через PromptFn. Провайдер подставляется снаружи
// (см. Server::getExecutor): исполнитель не знает ни о модели, ни о
// сервере, ни о сервисах — поэтому его можно создать и проверить
// без моделей и без сети. Новая задача = новый класс-исполнитель,
// а не ветка в чужом классе.
using PromptFn = std::function<std::string(const std::string& system,
                                           const std::string& text)>;

class Executor
{
    public:
        virtual ~Executor() = default;

        // text — СЫРОЙ текст запроса (нормализация и форматирование
        // запроса — работа исполнителя, не роутера), params — решение
        // classify(). Бросает исключение при сбое: вызывающий
        // превращает его в ResponseStatus.
        virtual std::string execute(const std::string& text,
                                    const Router::Params& params) = 0;
};

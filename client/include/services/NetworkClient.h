#pragma once

#include "NetworkError.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <zmq.hpp>

// Транспорт клиента: REQ-сокет ZeroMQ поверх TCP.
//
// Контракт (у NetworkServer контракт тот же — держи их одинаковыми):
//
//   - любая сетевая ошибка немедленно закрывает сокет: после неё
//     isConnected() == false, и соединение надо ставить заново через
//     connect(). Благодаря этому REQ не может остаться запертым в ожидании
//     ответа на давно брошенный запрос, а опоздавший ответ на старый запрос
//     с нового соединения уже не прилетит — кадры разных запросов не могут
//     смешаться.
//
// connect() в zmq асинхронен: он не ждёт пира и возвращает true даже тогда,
// когда сервера нет (порт закрыт, файрвол рвёт соединение). Достижимость
// проверяется первым же send()/receive() — при недоступном сервере там
// будет Timeout.
//
// Очерёдность строгая: отправив запрос, до ответа второй не отправить — zmq
// вернёт EFSM, это NetworkError::SocketError. Так и нужно: конвейер
// Sending -> Waiting это и есть «один запрос за раз».
class NetworkClient
{
    public:
        NetworkClient();
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        // tcp://<host>:<port>. Повторный вызов закрывает старое соединение.
        bool connect(const std::string& host, std::uint16_t port);

        // Закрыть соединение. lastError() не трогает: это результат
        // последней операции, а не текущего состояния сокета.
        void disconnect();

        bool isConnected() const;

        // Отправить один кадр. Содержимое сообщения переезжает в zmq,
        // поэтому параметр по значению: вызывающий отдаёт кадр, а не копию.
        bool send(zmq::message_t message);

        // Принять один кадр. nullopt = смотреть lastError()/lastErrorText().
        std::optional<zmq::message_t> receive();

        // Таймауты в миллисекундах: больше нуля — ждать столько, меньше
        // нуля — бесконечно. Менять можно и без соединения: значение
        // подхватит следующий connect(), на живом сокете применяется сразу.
        void setReceiveTimeout(int milliseconds);
        void setSendTimeout(int milliseconds);

        int receiveTimeout() const;
        int sendTimeout() const;

        NetworkError lastError() const;
        const std::string& lastErrorText() const;

    private:
        // Единственная точка отказа: записать причину и закрыть сокет.
        bool fail(NetworkError error, std::string text);

        // Перенести таймауты в сокет (см. setReceiveTimeout/setSendTimeout).
        void applyTimeouts();

        // Контекст объявлен раньше сокета, чтобы сокет закрывался первым:
        // иначе zmq_ctx_term в деструкторе контекста ждал бы освобождения
        // сокета, который уже уничтожен. unique_ptr, а не optional: у
        // zmq::socket_t нет конструктора по умолчанию, а reset() — это ровно
        // «закрыть соединение».
        zmq::context_t context;
        std::unique_ptr<zmq::socket_t> socket;

        NetworkError error = NetworkError::Ok;
        std::string errorText;

        // По умолчанию минута: ассистент на слабом железе думает долго, но
        // и вечное ожидание в WaitingState недопустимо.
        int receiveTimeoutMs = 60'000;
        int sendTimeoutMs = 10'000;
};

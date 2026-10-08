#pragma once

enum class Event
{
    BUTTON_PRESSED,
    BUTTON_RELEASED,

    RECORDING_FINISHED,
    SENDING_FINISHED,
    RESPONSE_RECEIVED,
    RESULT_SHOWN,

    // Сеть (шаг 2). Успешного исхода у этих двух нет: и таймаут, и обрыв
    // соединения ведут ровно в одну ошибку NETWORK_ERROR и в ErrorState,
    // так что handleEvent не спрашивает про success у таких событий.
    NETWORK_TIMEOUT,
    CONNECTION_LOST
};

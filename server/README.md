# BabelFish Server

Сервер принимает аудио или текст по ZeroMQ, распознавалкой **whisper.cpp**
получает текст, а LLM **llama.cpp** генерирует ответ. Всё локально.

## Запуск

```powershell
BabelFishServer.exe [port] [options]
```

Порт по умолчанию — **5555**. Модели загружаются **сразу при старте**, до
первого запроса: лог покажет тайминги, значит, всё встало на места и первый
клиент не будет ждать загрузку.

```
[server] compute devices:
  [0] CUDA0 - NVIDIA GeForce RTX 5070 Ti
  [1] CUDA1 - NVIDIA GeForce RTX 2070 SUPER
  [2] CPU - AMD Ryzen 5 7500F 6-Core Processor
[server] listening on tcp://*:5555
[server] transcriber: H:/models/ggml-large-v3-turbo.bin on gpu 1 (2.9 s)
[server] assistant: H:/models/Qwen3-4B-Q6_K.gguf on gpu 0 (1.8 s)
```

Список `[0] [1] [2]` сверху — это и есть те номера, что принимают
`--transcriber-gpu` и `--assistant-gpu`. Номера GPU могут отличаться от
системных: ggml нумерует только доступные бэкенды.

## Опции

| Опция | По умолчанию | Что делает |
| --- | --- | --- |
| `--transcriber-model <path>` | `H:/models/ggml-large-v3-turbo.bin` | Модель whisper (ggml) |
| `--transcriber-gpu <N\|cpu>` | `gpu 1` | Устройство для распознавания |
| `--assistant-model <path>` | `H:/models/Qwen3-4B-Q6_K.gguf` | Модель LLM (GGUF) |
| `--assistant-gpu <N\|cpu>` | `gpu 0` | Устройство для генерации ответа |
| `--assistant-layers <N\|all>` | `all` | Сколько слоёв уводить в VRAM |
| `--help` | — | Справка |

Значения `gpu N` / `cpu` можно писать просто как число или `cpu`.
Примеры:

```powershell
BabelFishServer.exe 8080
BabelFishServer.exe --transcriber-gpu cpu
BabelFishServer.exe --assistant-model H:/models/Qwen3-8B-Q4_K_M.gguf --assistant-gpu 0
BabelFishServer.exe --assistant-layers 40
```

## Раскладка по картам

По умолчанию whisper живёт на слабой карте, LLM — на сильной. Это сделано
намеренно: они работают **параллельно**, и загрузка на одну карту не мешает
другой.

```
 whisper ─► RTX 2070 (8 ГБ)   ~1.9 ГБ занято
 LLM     ─► RTX 5070 Ti       ~4.5 ГБ занято
```

Если карта в списке устройств отсутствует, сервер **падает с ошибкой**, а не
молча уходит на CPU. Это специально: тихий откат на процессор выглядел бы как
«всё работает, но почему-то медленно».

## `--assistant-layers` для тяжёлых моделей

Когда модель не помещается в VRAM целиком, можно увести в неё часть слоёв, а
остальное держать в оперативной:

| Значение | Поведение |
| --- | --- |
| `all` | Все слои в VRAM (по умолчанию) |
| `0` | Никакие слои не на GPU, всё на CPU |
| `N` | N слоёв в VRAM, остальное в RAM |

Например, квантованная 32B-модель не влезет в 16 ГБ целиком, но с
`--assistant-layers 40` отлично работает: часть слоёв на GPU, остаток в RAM.
Скорость падает пропорционально доле CPU-слоёв, но для субтитров обычно
хватает.

Работает только при `assistantDevice >= 0` — на CPU разделение слоёв бессмысленно.

## Требования

CUDA-сборка нуждается в:

- Visual Studio Build Tools 2022 (workload `VCTools`)
- CUDA Toolkit 13.4
- `BABELFISH_WITH_CUDA=ON` при конфигурации

Собрать проще всего скриптом из корня репозитория:

```powershell
.\build-cuda.bat --target BabelFishServer
```

Он сам поднимает окружение MSVC, находит nvcc и кладёт runtime-DLL рядом с
exe. Без CUDA доступна обычная сборка (`build/`), но там модели пойдут на CPU.

> **Про DLL:** при ручной сборке каталоги `build-cuda\bin` и
> `CUDA\v13.4\bin\x64` должны быть в PATH — иначе запуск упадёт с
> `0xC0000135`. В CUDA 13.x x64-DLL лежат в `bin\x64`, а не в `bin`.

## Порядок работы с клиентом

Сервер нужно поднимать **первым** — клиент при старте сразу пытается
подключиться. Для удалённого клиента разреши входящий TCP 5555 в
брандмауэре.

Подробности клиента — в `../client/README.md`.

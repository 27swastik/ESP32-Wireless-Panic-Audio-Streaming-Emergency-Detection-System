# realtime_stt.py
import sys
import json
from vosk import Model, KaldiRecognizer

model = Model("vosk-model-small-en-us-0.15")
rec = KaldiRecognizer(model, 16000)

last_partial = ""

while True:
    data = sys.stdin.buffer.read(4000)
    if len(data) == 0:
        break

    if rec.AcceptWaveform(data):
        result = json.loads(rec.Result())
        text = result.get("text", "")
        if text:
            print(text, flush=True)

    else:
        partial = json.loads(rec.PartialResult())
        partial_text = partial.get("partial", "")
        # ✅ Only print if it's new
        if partial_text and partial_text != last_partial:
            print(partial_text, flush=True)
            last_partial = partial_text

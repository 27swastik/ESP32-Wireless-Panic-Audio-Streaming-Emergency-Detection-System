// public/processor.js
class PCMPlayer extends AudioWorkletProcessor {
    constructor() {
      super();
      this.buffer = [];
      this.port.onmessage = (event) => {
        if (event.data.type === "push") {
          this.buffer.push(...event.data.samples);
        }
      };
    }
  
    process(inputs, outputs) {
        const output = outputs[0][0];
        const ratio = sampleRate / 16000; // 3.0 if AudioContext is 48kHz
        for (let i = 0; i < output.length; i++) {
          const srcIndex = Math.floor(i / ratio);
          output[i] = this.buffer.length > srcIndex ? this.buffer[srcIndex] : 0;
        }
        this.buffer.splice(0, Math.floor(output.length / ratio));
        return true;
    }
      
}
  
registerProcessor("pcm-player", PCMPlayer);
  
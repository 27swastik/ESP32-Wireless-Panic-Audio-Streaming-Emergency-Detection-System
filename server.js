const fs = require('fs');
const http = require('http');
const WebSocket = require('ws');
const express = require('express');
const path = require('path');
const { spawn } = require('child_process');




const TELEGRAM_BOT_TOKEN = "TOKEN"; //  Replace with real token
const TELEGRAM_CHAT_ID = "CHAT_ID";     //  Replace with your chat ID
//  Replace with your server URL

function sendTelegramMessage(text) {
  const url = `https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendMessage`;
  const body = {
    chat_id: TELEGRAM_CHAT_ID,
    text,
    parse_mode: "MarkdownV2"
  };
  const escapeMarkdown = (t) =>
    t.replace(/[_*[\]()~`>#+=|{}.!-]/g, '\\$&');

  body.text = escapeMarkdown(text);

  fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  }).then(res => res.json())
    .then(json => {
      if (!json.ok) console.error("Telegram Error:", json);
    });
}


const app = express();
app.use(express.static(path.join(__dirname, 'public')));
app.use('/audio', express.static(path.join(__dirname, 'audio')));
app.use('/data', express.static(path.join(__dirname, 'data')));

const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

let currentAudioStream = null;
let currentCSV = null;
let currentSessionTimestamp = null;

//  Launch VOSK subprocess
const vosk = spawn('python', ['realtime_stt.py']);
vosk.stdout.on("data", (chunk) => {
  const msg = chunk.toString().trim();
  console.log(" Vosk:", msg);
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(JSON.stringify({ type: 'transcript', text: msg }));
    }
  });
  if (msg.toLowerCase().includes("help") || msg.toLowerCase().includes("fire")) {
    console.log("🚨 EMERGENCY DETECTED!");
    const alertText = `🚨 *Emergency keyword detected!*\n \"${msg.trim()}\"\n ${new Date().toLocaleString()}`;
    sendTelegramMessage(alertText); 
    wss.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(JSON.stringify({ type: 'alert', keyword: msg }));
      }
    });
  }
});



wss.on('connection', function connection(ws) {
  console.log('📡 ESP32 connected');
  ws.send(JSON.stringify({ type: "status", value: "esp_connected" }));

  ws.on('message', function incoming(data, isBinary) {
    // 🔄 Handle audio binary data
    if (isBinary) {
      if (currentAudioStream) currentAudioStream.write(data);
      if (vosk.stdin.writable) vosk.stdin.write(data);
      // ✅ Broadcast audio to browser clients
      wss.clients.forEach(client => {
        if (client !== ws && client.readyState === WebSocket.OPEN) {
          client.send(data);
        }
      });
      return;
    }
    

    //  Handle JSON messages
    try {
      const text = data.toString();
      console.log('📥 Message:', text);

      const message = JSON.parse(text);

      if (message.type === 'start') {
        function getLocalTimestamp() {
          const now = new Date();
          return now.getFullYear() + "-" +
                 String(now.getMonth() + 1).padStart(2, '0') + "-" +
                 String(now.getDate()).padStart(2, '0') + "_" +
                 String(now.getHours()).padStart(2, '0') + "-" +
                 String(now.getMinutes()).padStart(2, '0') + "-" +
                 String(now.getSeconds()).padStart(2, '0');
        }
        
        const timestamp = getLocalTimestamp();
        
        const wavFile = path.join(__dirname, `audio/panic_${timestamp}.wav`);
        const csvFile = path.join(__dirname, `data/sensor_${timestamp}.csv`);

        console.log(`🎙️ New session: ${timestamp}`);
        currentAudioStream = fs.createWriteStream(wavFile);
        currentCSV = fs.createWriteStream(csvFile);
        currentCSV.write("timestamp,temp,humidity,mic_peak\n");
        currentSessionTimestamp = timestamp;

        writeWavHeader(currentAudioStream);
        const alertMsg = `🚨 *Emergency audio stream started!*\n🕒 _${timestamp}_\n`;
        for (let i = 0; i < 3; i++) {
          sendTelegramMessage(alertMsg);
        }

      } else if (message.type === 'stop') {
        console.log("⛔ Session stopped");
        if (currentAudioStream) currentAudioStream.end();
        if (currentCSV) currentCSV.end();
        currentAudioStream = null;
        currentCSV = null;
        currentSessionTimestamp = null;

      } else if (message.type === 'sensor') {
        if (currentCSV) {
          const line = `${Date.now()},${message.temp},${message.hum},${message.mic_peak}\n`;
          currentCSV.write(line);
        }
        wss.clients.forEach(client => {
          if (client !== ws && client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(message));
          }
        });
      }
      else if (message.type === 'status') {
        // Broadcast status to all clients (browser dashboards)
        wss.clients.forEach(client => {
          if (client !== ws && client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ type: 'status', value: message.value }));
          }
        });
      }
      

    } catch (err) {
      console.error('❌ Invalid JSON:', err.message);
    }
  });
});

function writeWavHeader(stream) {
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(0, 4); // Placeholder for ChunkSize
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16); // Subchunk1Size
  header.writeUInt16LE(1, 20);  // PCM format
  header.writeUInt16LE(1, 22);  // Mono
  header.writeUInt32LE(16000, 24); // Sample rate
  header.writeUInt32LE(16000 * 2, 28); // Byte rate
  header.writeUInt16LE(2, 32);  // Block align
  header.writeUInt16LE(16, 34); // Bits per sample
  header.write('data', 36);
  header.writeUInt32LE(0, 40); // Placeholder for data size

  stream.write(header);
}

// API to list audio recordings
app.get('/api/audio-files', (req, res) => {
  const audioDir = path.join(__dirname, 'audio');
  fs.readdir(audioDir, (err, files) => {
    if (err) {
      console.error('❌ Error reading audio directory:', err);
      return res.status(500).json({ error: 'Unable to read audio folder' });
    }
    const wavFiles = files.filter(f => f.endsWith('.wav'));
    res.json(wavFiles);
  });
});

// API to list CSV files
app.get('/api/csv-files', (req, res) => {
  const dir = path.join(__dirname, 'data');
  fs.readdir(dir, (err, files) => {
    if (err) {
      console.error('❌ Failed to read data folder:', err);
      return res.status(500).json({ error: 'Error reading CSV files' });
    }
    const csvs = files.filter(f => f.endsWith('.csv'));
    res.json(csvs);
  });
});

server.listen(8000,"0.0.0.0", () => {
  console.log(` Server running at http://localhost:8000`);
});

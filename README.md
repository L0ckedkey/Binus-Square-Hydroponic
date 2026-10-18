# Binus Square Hydroponic – Node-RED Flow

Flow Node-RED untuk menerima data sensor dari ESP32 (suhu air, suhu udara, kelembapan) lalu menampilkannya di dashboard.

```
ESP32 (DS18B20 + DHT22) --HTTP POST--> Node-RED /sensor-data --> Dashboard /dashboard
```

## Isi repo

| File | Keterangan |
| --- | --- |
| `flows.json` | Flow Node-RED (endpoint sensor + dashboard) |
| `hydroponic_monitor_esp32.txt` | Code ESP32 (salin ke Arduino IDE sebagai `.ino`) |
| `wiring_hydroponic_monitor.png` | Skema wiring ESP32, DS18B20, DHT22 |

## Kebutuhan

- Node-RED versi 3.x atau lebih baru
- Palette **`@flowfuse/node-red-dashboard`** (Dashboard 2.0). Node `ui-base`, `ui-page`, `ui-chart` dan `ui-template` berasal dari palette ini.

Install palette lewat **Menu → Manage palette → Install**, cari `@flowfuse/node-red-dashboard`. Bisa juga lewat terminal di folder data Node-RED:

```bash
npm install @flowfuse/node-red-dashboard
```

## Cara import flow

**Opsi A – lewat editor (disarankan)**

1. Buka editor Node-RED, misalnya `http://<ip-server>:1880`.
2. **Menu (☰) → Import**, lalu pilih file `flows.json` atau paste isinya.
3. Pilih **Import to: new flow**. Pilih **Replace** kalau flow lama ingin ditimpa.
4. Klik **Deploy**.

**Opsi B – timpa file langsung**

```bash
cd ~/node_red_bsq/node-red-data
cp flows.json flows.json.bak          # backup flow lama
cp /path/ke/repo/flows.json flows.json
# lalu restart Node-RED, contoh jika memakai Docker:
docker restart <nama-container-node-red>
```

## Endpoint sensor

| | |
| --- | --- |
| Method | `POST` |
| URL | `https://hydroponic.hansprojects.com/sensor-data` (atau `http://<ip-server>:1880/sensor-data` di jaringan lokal) |
| Header | `Content-Type: application/json` |
| Respon | `200 OK` |

Contoh body:

```json
{
  "device": "hydro-esp32-01",
  "water_temp_c": 27.31,
  "air_temp_c": 30.2,
  "humidity_percent": 71.5,
  "rssi": -61,
  "uptime_s": 1234
}
```

Field yang dipakai flow: `water_temp_c`, `air_temp_c`, dan `humidity_percent`. Field lain diabaikan. Nilai `null` akan tampil sebagai **N/A**.

Tes tanpa ESP32:

```bash
curl -X POST https://hydroponic.hansprojects.com/sensor-data \
  -H "Content-Type: application/json" \
  -d '{"water_temp_c":25.4,"air_temp_c":29.1,"humidity_percent":65}'
```

## Dashboard

Buka `https://hydroponic.hansprojects.com/dashboard`. Halaman **Hydroponics** berisi:

- **Kartu status**: suhu udara, suhu air, dan kelembapan, dengan warna status.
- **Water & Air Temperature History**: grafik suhu selama 1 jam terakhir.
- **Humidity History**: grafik kelembapan selama 1 jam terakhir.

Batas status yang dipakai (diatur di function node **Evaluate status**):

| Parameter | Normal | Di luar rentang |
| --- | --- | --- |
| Suhu udara | 20–30 °C | Warning (merah) |
| Suhu air | 18–26 °C | Warning (merah) |
| Kelembapan | 50–70 % | Warning (merah) |

## Struktur flow

```
Sensor Input (http in, POST /sensor-data)
 ├─ Reply 200 OK (http response)
 ├─ Remap topic water and air ─> Water & Air Temperature History (chart)
 ├─ Remap humidity ───────────> Humidity History (chart)
 └─ Evaluate status ──────────> HTML (kartu status)
```

## Troubleshooting

- **Node berwarna merah atau bertanda "unknown" setelah import**: palette `@flowfuse/node-red-dashboard` belum terpasang.
- **Request dari ESP32 timeout**: pastikan node **Reply 200 OK** tersambung ke **Sensor Input**.
- **Grafik kosong, kartu status N/A**: nama field di payload tidak sama dengan `water_temp_c`, `air_temp_c`, atau `humidity_percent`. Cek dengan menambahkan node `debug` setelah **Sensor Input**.
- **Bisa dari lokal, tidak bisa lewat domain**: cek Cloudflare Tunnel sudah mengarah ke port Node-RED (default `1880`).

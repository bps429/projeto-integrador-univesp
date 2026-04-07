# Firmware — CEMEI Zacarelli

## Como gravar na placa

### 1. Instalar o Arduino IDE
Baixe em: https://www.arduino.cc/en/software

### 2. Adicionar suporte ao ESP32
- Abra: Arquivo → Preferências
- Em "URLs adicionais", cole:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Ferramentas → Placa → Gerenciador de placas
- Pesquise `esp32` → instale **esp32 by Espressif Systems**

### 3. Selecionar a placa correta
- Ferramentas → Placa → `ESP32 DevKit V1`
- Ferramentas → Porta → (a porta COM que aparecer ao conectar o USB)

### 4. Instalar as bibliotecas
- Sketch → Incluir Biblioteca → Gerenciar Bibliotecas
- Instale:
  - `VL53L0X` by **Pololu**
  - `ArduinoJson` by **Benoit Blanchon**

### 5. Gravar o código
- Abra o arquivo `cemei_zacarelli.ino`
- Clique em **Upload** (seta →)

---

## Primeiro uso (WiFiManager)

1. Após gravar, a placa vai ligar e **não encontrar WiFi salvo**
2. Ela cria um hotspot chamado **`CEMEI-Config`** (senha: `cemei1234`)
3. Conecte seu celular nesse hotspot
4. O celular vai abrir automaticamente a página de configuração
   - Se não abrir, acesse: `http://192.168.4.1`
5. Digite o nome e senha do WiFi desejado
6. Clique **Salvar e Conectar**
7. A placa reinicia e conecta automaticamente ✅

> Para trocar de WiFi depois: segure o botão **BOOT** enquanto aperta **RESET**

---

## Ajuste de calibração

Edite no início do arquivo `.ino`:

```cpp
const int ALTURA_SENSOR_CM = 220; // altura do sensor até o chão (meça com fita)
const int ESTATURA_MIN = 60;      // estatura mínima para considerar criança
const int ESTATURA_MAX = 140;     // estatura máxima para considerar criança
```

---

## Pinagem

| VL53L0X | ESP32 DevKit V1   |
|---------|-------------------|
| VIN     | 3V3               |
| GND     | GND               |
| SDA     | D21               |
| SCL     | D22               |

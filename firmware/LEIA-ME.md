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
- Ferramentas → Placa → `ESP32C3 Dev Module`
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

## Solução de Problemas — Portal Wi-Fi (SSID CEMEI-Config)

Se após gravar o firmware a rede **CEMEI-Config** não aparecer no celular, ou você não conseguir abrir **192.168.4.1**, siga os passos abaixo:

### 1. Verifique se o Serial Monitor confirma o portal ativo
Abra o Serial Monitor (115200 baud) e procure estas linhas:
```
*wm:StartAP with SSID:  CEMEI-Config
*wm:AP IP address: 192.168.4.1
*wm:Starting Web Portal
```
Se elas aparecerem, o portal está rodando — o problema é apenas na conexão do celular.

### 2. Use apenas redes 2.4 GHz
O ESP32-C3 **não suporta 5 GHz**. O celular precisa estar conectado na **faixa 2.4 GHz** para enxergar a rede CEMEI-Config e para poder configurar o Wi-Fi da escola/casa.

- No Android: Configurações → Wi-Fi → verifique se há uma opção "2.4 GHz" ou "Frequência"
- No iOS: conecte-se a uma rede 2.4 GHz e depois escaneie novamente

### 3. Verifique a alimentação da placa
Uma alimentação insuficiente pode fazer o ESP32 reiniciar antes de subir o portal.

- Use o cabo USB diretamente no computador (evite hubs USB sem alimentação)
- Certifique-se de que o cabo não é somente de carregamento (precisa de dados)
- LEDs de alimentação da placa devem estar acesos

### 4. Reinicie a placa corretamente
Se a placa travou em outro estado, force um reset:

1. Pressione e segure o botão **BOOT**
2. Enquanto segura, pressione e solte o **RESET** (ou reconecte o USB)
3. Solte o **BOOT**
4. A placa vai reiniciar e deve aparecer `StartAP with SSID: CEMEI-Config` no Serial Monitor

### 5. Rede não aparece no celular
- Desligue o Wi-Fi do celular e ligue novamente
- Aproxime o celular a menos de 2 metros do ESP32
- Aguarde até 30 segundos para a rede aparecer na lista
- Reinicie o celular se a rede ainda não aparecer

### 6. Rede aparece mas não redireciona automaticamente
Alguns dispositivos não ativam o portal cativo automaticamente. Nesse caso:

1. Conecte-se à rede **CEMEI-Config** (senha: `cemei1234`)
2. Abra o navegador (Chrome ou Safari)
3. Digite manualmente: **http://192.168.4.1** (não https)
4. A página de configuração do Wi-Fi deve aparecer

> ⚠️ **Importante:** use `http://` (não `https://`). Alguns navegadores adicionam `https://` automaticamente — nesse caso o portal não abrirá.

### 7. Mensagem de "sem internet" no celular
Ao conectar na rede CEMEI-Config, o celular pode mostrar "Rede sem acesso à internet" ou similar — isso é **normal**. O portal não tem internet, é apenas um ponto de acesso local para configuração. Aceite a conexão e continue.

---

## Ajuste de calibração

Edite no início do arquivo `.ino`:

```cpp
const int ALTURA_SENSOR_CM = 200; // altura do sensor até o chão (meça com fita)
const int ESTATURA_MIN = 60;      // estatura mínima para considerar criança
const int ESTATURA_MAX = 140;     // estatura máxima para considerar criança
```

---

## Pinagem

| VL53L0X | ESP32-C3 SuperMini |
|---------|-------------------|
| VCC     | 3.3V              |
| GND     | GND               |
| SDA     | GPIO 6            |
| SCL     | GPIO 7            |
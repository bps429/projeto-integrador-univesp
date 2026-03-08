# Projeto Integrador UNIVESP — CEMEI Zacarelli

## Sistema IoT de Monitorização de Estatura para Prevenção de Evasão Escolar

### Descrição
Sistema IoT desenvolvido para o CEMEI Zacarelli que detecta crianças na porta de entrada pela estatura, enviando alertas em tempo real para a secretaria via dashboard web.

### Arquitetura
[VL53L0X] → [ESP32-C3 SuperMini] → [Wi-Fi] → [Firebase] → [Dashboard Web]

### Componentes de Hardware
- ESP32-C3 SuperMini
- Sensor laser VL53L0X (I2C)
- Terminal Shield de expansão
- Power Bank 10.000mAh

### Funcionamento
- Sensor instalado a 200 cm do chão, apontando para baixo
- Mede distância até a pessoa na porta
- **Estatura = 200 − distância medida**
- Se estatura entre **60 e 140 cm** → Criança → envia alerta
- Se fora desse range → Adulto → status OK

### Dashboard Web
Acesse: https://bps429.github.io/projeto-integrador-univesp/dashboard/

### Firebase
- Projeto: integrador-univesp
- Nó de dados: /leitura

### Estrutura do Repositório
projeto-integrador-univesp/
- dashboard/index.html
- dashboard/style.css
- dashboard/app.js
- firmware/esp32c3_vl53l0x_final.ino
- README.md

### Equipe
UNIVESP — Projeto Integrador 2026

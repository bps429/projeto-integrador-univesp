# Projeto Integrador UNIVESP — CEMEI Zacarelli

## Sistema IoT de Monitorização de Estatura para Prevenção de Evasão Escolar

### Descrição
Sistema IoT desenvolvido para o CEMEI Zacarelli que detecta crianças na porta de entrada pela estatura, enviando alertas em tempo real para a secretaria via dashboard web.

### Arquitetura
[VL53L0X] → [ESP32 DevKit V1] → [Wi-Fi] → [Firebase] → [Dashboard Web]

### Componentes de Hardware
- ESP32 DevKit V1
- Sensor laser VL53L0X (I2C)
- Power Bank 5.000mAh

### Funcionamento
- Sensor instalado a 200 cm do chão, apontando para baixo
- Mede distância até a pessoa na porta
- **Estatura = 220 − distância medida**
- Se estatura entre **30 e 120 cm** → Criança → envia alerta
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
- firmware/cemei_zacarelli.ino
- README.md

### Equipe
UNIVESP — Projeto Integrador 2026

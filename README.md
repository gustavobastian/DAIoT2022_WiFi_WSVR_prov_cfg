# ESP32 Wi-Fi Config

## Configuración de SSID y PASSWD a través de webserver.

1. Compilar la aplicación y cargar firmware en ESP32.

2. Al iniciar genera un punto de acceso (AP) con la siguiente configuración por defecto: 

    #define DEFAULT_AP_SSID "DAIoT_ESP32_WS"

    #define DEFAULT_AP_IP "192.168.4.1"

3. Al conectarse a la red Wi-Fi que genera, debemos ingresar en un navegador la IP.

4. En el navegador se abrirá un formulario que permitirá completar SSID y PASSWD de la red Wi-Fi
a la que debería conectarse el ESP32

5. El led onboard del modulo de desarrollo espdevkit-c encenderá fijo cuando el dispositivo logre conexión o parpadeará en caso contrario
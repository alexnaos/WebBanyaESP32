#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>ESP32 Control</title>
    <style>
        body { font-family: sans-serif; text-align: center; background: #f4f4f4; }
        .card { background: white; padding: 20px; border-radius: 10px; display: inline-block; margin-top: 50px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .status { font-weight: bold; font-size: 1.2em; }
        .on { color: green; } .off { color: red; }
    </style>
    <script>
        setInterval(function() {
            fetch('/data').then(response => response.json()).then(data => {
                document.getElementById('led').innerHTML = data.led ? 'ON' : 'OFF';
                document.getElementById('led').className = 'status ' + (data.led ? 'on' : 'off');
                document.getElementById('pwm').innerHTML = data.mosfet;
            });
        }, 2000);
    </script>
</head>
<body>
    <div class='card'>
        <h2>ESP32 Malek Monitor</h2>
        <p>LED Status: <span id='led' class='status'>--</span></p>
        <p>MOSFET Level: <span id='pwm' class='status'>--</span></p>
    </div>
</body>
</html>
)=====";

#endif

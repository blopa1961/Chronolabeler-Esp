const char wifiCfg[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Configuraci&oacute;n de WiFi</title>
</head>
<style>
input[type="submit"] {
  height: 50px;
  width: 250px;
  font-size: 20px;
}
input[type="button"] {
  height: 50px;
  width: 250px;
  font-size: 20px;
}
input[type="text"] {
  font-size: 20px;
}
input[type="number"] {
  font-size: 20px;
}
input[type="password"] {
  font-size: 20px;
}
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
table, th, td {border: 1px solid black;  border-collapse: collapse;}
</style>
<body>
<p style="font-size:25px"><b>Configuraci&oacute;n de Conexi&oacute;n</b></p><br>
<form action="/save_Wifi_config" method="POST">
  <p style="font-size:20px"><b>
  <label for="elssid">SSID:</label><br>
  <input type="text" maxlength="32" id="elssid" name="mySSID" placeholder= "Ingrese SSID" value="defaultSSID"><br>
  <label for="elpass">Password:</label><br>
  <input type="password" maxlength="64" id="elpass" name="myPASS" placeholder= "Ingrese Password" value="defaultPASS"><br>
  <label for="elip">IP:</label><br>
  <input type="text" maxlength="15" id="elip" name="myIP" placeholder= "Ingrese IP (DHCP 0.0.0.0)" value="staticIP"><br>
  <label for="elgate">Gateway:</label><br>
  <input type="text" maxlength="15" id="elgate" name="myGate" placeholder= "Ingrese Gateway" value="myGateway"><br>
  <label for="lamask">M&aacute;scara:</label><br>
  <input type="text" maxlength="15" id="lamask" name="myMask" placeholder= "Ingrese M&aacute;scara" value="subnet"><br>
  <label for="eltime">Reintentar (minutos):</label><br>
  <input type="number" min="0" maxlength="2" id="eltime" name="timeout" placeholder= "Ingrese Timeout" value="TOUT"><br>
  <br>Modo de Inicio de WiFi:<br></b>
  <input type="radio" id="rout" name="inicio" value="router"CHECKRTR>
  <label for="rout">Conectado a Router</label><br>
  <input type="radio" id="apm" name="inicio" value="ap"CHECKAPM>
  <label for="apm">Access Point local</label><br>
  <input type="radio" id="apag" name="inicio" value="off"CHECKOFF>
  <label for="apag">M&oacute;dulo Apagado</label><br></b></p>
  <br><br>
  <input type="submit" value="Guardar configuraci&oacute;n"><br><br>
</form>
<input type="button" value="Cancelar" onclick="history.go(-1)"><br>
PLACEHLD
</body>
</html>
)=====";

const char CfgHeader[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
table, th, td {border: 1px solid black;  border-collapse: collapse; font-size: 20px;}
input[type="button"] {
  height: 50px;
  width: 220px;
  font-size: 20px;
}
</style>
</head>
<body>
<p style="font-size:20px"><b>Configuraci&oacute;n de WiFi:</b></p><br>
)=====";

const char ScrollEnd[] PROGMEM = R"=====(
<script>window.addEventListener('load', () => {window.scrollTo({top: document.body.scrollHeight+50, behavior: 'smooth'});});</script>
)=====";
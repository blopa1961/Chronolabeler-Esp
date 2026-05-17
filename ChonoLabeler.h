const char PaginaWeb[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<link rel="icon" href="/favicon32.png" sizes="32x32" type="image/png">
<link rel="icon" href="/favicon192.png" sizes="192x192" type="image/png">
<title>Chrono Labeler</title>
<script>
setInterval(function() {
  getEstado();
}, 500);

function getEstado() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) document.getElementById("estado").innerHTML = this.responseText;
  };
  xhttp.open("GET", "updEstado", true);
  xhttp.send();
}
</script>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
button[type="submit"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
</style>
</head>
<body>
<p style="font-family:Arial;font-size:30px"><b>Chrono Labeler</b><br><br></p>
<form action="/" method="POST">
  <p style="font-size:20px"><b>Perfil:</b><br>
  <select style="font-size:17px" name="selPrn" onchange="this.form.submit()">
  PULLOPTIONS
  </select><br>
  <p style="font-size:20px"><b>Estado: </b><span id="estado">Desconectada</span></p><br>
  <input type="text" style="font-size:20px" maxlength="20" id="linea1" name="line0" placeholder= "Texto a imprimir (l&iacute;nea 1)" value="MYLINE0"><br><br>
  <input type="text" style="font-size:20px" maxlength="20" id="linea2" name="line1" placeholder= "Texto a imprimir (l&iacute;nea 2)" value="MYLINE1"><br><br>
  <p style="font-size:20px"><b>Fuente l&iacute;nea 1:</b><br>
  <select style="font-size:17px" name="pullf0">
  PULLFR0
  </select><br><br>
  <p style="font-size:20px"><b>Fuente l&iacute;nea 2:</b><br>
  <select style="font-size:17px" name="pullf1">
  PULLFR1
  </select><br><br></p>
  <p style="font-size:20px"><b>
  <label for="centro">Centrar textos:&nbsp</label>
  <input type="checkbox" id="centro" name="centrar" CENTERCHECK><br><br>
  <label>Fondo: </label></b>
  <input type="radio" id="blanco" name="color" value="0"CHECKWHITE>
  <label for="blanco">Blanco</label>
  <input type="radio" id="negro" name="color" value="1"CHECKBLACK>
  <label for="negro">Negro</label><br><br><br></p><b>
  &nbsp&nbsp<button type="submit" formaction="/preview" name= "prvw" value="7">Previsualizar</button><br><br>
  &nbsp&nbsp<button type="submit" name="accion" value="1" PRINTENA>Imprimir</button><br><br>
  </form>
  <br><p style="font-size:20px"><b><a href="/configPrn">Configurar Impresora</a></b>
  <br><br><b><a href="/configDate">Configurar Fecha y Hora</a></b>
  <br><br><b><a href="/configWifi">Configurar WiFi</a></b>
  <br><br><b><a href="/about">Acerca de</a></b></p>
  <br><p style="font-size:17px"><a href="http://www.chronolabeler.com">&copy; 2026 Pablo Montoreano</a></p>
</body>
</html>
)=====";

const char ConfPrn[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Configuraci&oacute;n de la Impresora</title>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
button[type="submit"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
input[type="button"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
input[type="text"] {
  font-size: 15px;
}
table, th, td {border: 1px solid black;  border-collapse: collapse;}
</style>
</head>
<body>
<p style="font-size:25px"><b>Configuraci&oacute;n de Impresora</b></p><br>
<form action="/configPrn" method="POST">
  <select style="font-size:15px" name="selCfgPrn" onchange="this.form.submit()">
  PULLCFG
  </select><br>
</form>
<form action="/" method="POST">
  <p style="font-size:20px"><b>
  <label for="elprofile">Perfil de la impresora NUMPERF</label><br>
  <input type="text" maxlength="25" id="elprofile" name="myProfile" placeholder= "Perfil de la impresora" value="myPROF"><br>
  <label for="elname">Nombre Bluetooth:</label><br>
  <input type="text" maxlength="29" id="elname" name="myBleName" placeholder= "Nombre BLE de impresora" value="myNAME"><br>
  <label>Tipo: </label></b>
  <input type="radio" id="portr" name="prnType" value="0"CHECKPORT>
  <label for="portr">Vertical</label>
  <input type="radio" id="landsc" name="prnType" value="1"CHECKLAND>
  <label for="landsc">Apaisada</label><br><b>
  <label>Conexi&oacute;n:</label></b>
  <span id="vincu">PAIRED&nbsp;Vinculada</span><br><br><b>
  <label>Fuente L&iacute;nea 1:</label><br></b>
  <select style="font-size:15px" name="pullf0">
  PULLFR0
  </select><br><b>
  <label>Fuente L&iacute;nea 2:</label><br></b>
  <select style="font-size:15px" name="pullf1">
  PULLFR1
  </select><br>
  <p style="font-size:20px">
  <b><label>Fondo: </label></b>
  <input type="radio" id="blanco" name="color" value="0"CHECKWHITE>
  <label for="blanco">Blanco</label>
  <input type="radio" id="negro" name="color" value="1"CHECKBLACK>
  <label for="negro">Negro</label><br><br><b>
  <label for="elwidthmm">Ancho (mm):</label><br>
  <input type="number" min="30" max="56" id="elwidthmm" name="anchomm" placeholder= "Ancho en mm" value="WMM"><br>
  <label for="elheightmm">Alto (mm):</label><br>
  <input type="number" min="12" max="25" id="elheightmm" name="altomm" placeholder= "Alto en mm" value="HMM"><br>
  <label for="elpiemm">Ajuste pie de p&aacute;gina (mm):</label><br>
  <input type="number" min="0" max="80" id="elpiemm" name="piemm" placeholder= "Pie de p&aacute;gina" value="EXFT"><br>
  <label for="elmargensup1">Margen superior l&iacute;nea 1 (pixels):</label><br>
  <input type="number" min="0" max="MAXSUP" id="elmargensup1" name="margensup1" placeholder= "Margen superior l&iacute;nea 1" value="MS1"><br>
  <label for="elmargensup2">Margen superior l&iacute;nea 2 (pixels):</label><br>
  <input type="number" min="0" max="MAXSUP" id="elmargensup2" name="margensup2" placeholder= "Margen superior l&iacute;nea 2" value="MS2"><br>
  <label for="elmargenizq">Margen izquierdo (pixels):;</label><br>
  <input type="number" min="0" max="200" id="elmargenizq" name="margenizq" placeholder= "Margen izquierdo" value="MIZQ">&nbsp&nbsp
  <input type="checkbox" id="centro" name="centrar" CENTERCHECK></b>
  <label for="centro">Centrar</label><br><b>
  <label for="elchunkdelay">Retardo entre paquetes BLE (ms):</label><br>
  <input type="number" min="0" max="50" id="elchunkdelay" name="chunkdelay" placeholder= "Retardo paquetes (ms)" value="CDELAY"><br>
  <label for="elresetdelay">Retardo despu&eacute;s de reset (ms):</label><br>
  <input type="number" min="0" max="9999" id="elresetdelay" name="resetdelay" placeholder= "Retardo Reset (ms)" value="RDELAY"><br>
  <br><br>
  <button type="submit" formaction="/preview" name= "prvw" value="8">Previsualizar</button><br><br>
  <button type="submit" name="accion" value="2">Guardar</button><br><br>
  <button type="submit" name="accion" value="3">Eliminar</button><br><br>
  <button type="submit" name="accion" value="4">Cancelar</button><br>
</form>
PLACEHLD
</body>
</html>
)=====";

const char ConfDate[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Configuraci&oacute;n de fecha y hora</title>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 220px;
  font-size: 20px;
}
button[type="submit"] {
  height: 50px;
  width: 220px;
  font-size: 20px;
}
input[type="text"] {
  font-size: 15px;
}
</style>
</head>
<body>
<p style="font-size:25px"><b>Configuraci&oacute;n Fecha y Hora</b></p><br>
<form action="/" method="POST">
  <p style="font-size:20px"><b>Formato Fecha:</b></p>
  <p style="font-size:15px">
  <select name="pulldt0">
  PULLDR0
  </select>
  &nbsp;
  <select name="pulldt1">
  PULLDR1
  </select>
  &nbsp;
  <select name="pulldt2">
  PULLDR2
  </select><br></p>
  <p style="font-size:20px"></b>Separadores Fecha:<br></p>
  <p style="font-size:15px">
  <select name="pulldt5">
  PULLSEP0
  </select></p>
  <p style="font-size:20px">
  <input type="checkbox" id="diasem" name="diasema" DIASECHECK>
  <label for="diasem">D&iacute;a de la Semana</label><br><br>
  <p style="font-size:20px"><b>Formato Hora:</b></p>
  <p style="font-size:15px">
  <select name="pulldt3">
  PULLTR0
  </select>
  &nbsp;
  <select name="pulldt4">
  PULLTR1
  </select><br></p>
  <p style="font-size:20px"></b>Separadores Hora:&nbsp;</p>
  <p style="font-size:15px">
  <select name="pulldt6">
  PULLSEP1
  </select><br><br>
  <b></p>
  <p style="font-size:20px">
  <label for="elntp"><b>Servidor NTP:</b></label><br>
  <input type="text" style="font-size:15px" maxlength="30" id="elntp" name="myNtp" placeholder= "Servidor NTP" value="NTPR"><br></b>
  <p style="font-size:20px">Puede seleccionar uno de estos:<br>
  <select style="font-size:15px" name="pullntp" onchange="document.getElementById('elntp').value= this.options[this.selectedIndex].text">
  PULLNTP
  </select><br><br><b>
  <label for="eltzone">Zona NTP:</label><br>
  <input type="text" style="font-size:15px" maxlength="20" id="eltzone" name="myTZ" placeholder= "Zona de tiempo NTP" value="NTPTZR"><br>
  <label for="elhuso">Huso horario (UTC):</label><br>
  <input type="number" style="font-size:15px" min="-12" max="14" id="elhuso" name="huso" placeholder= "Huso horario" value="HUSOR"><br>
  <label for="elverano">Horario de verano:</label><br>
  <input type="number" style="font-size: 15px" min="-1" max="2" id="elverano" name="verano" placeholder= "Horario de Verano" value="VERAR"><br><br>
  <button type="submit" formaction="/preview" name= "prvw" value="9">Previsualizar</button><br><br>
  <button type="submit" name="accion" value="5">Guardar</button><br><br>
</form>
  <input type="button" value="Cancelar" onclick="window.location.href='/'"><br>
</body>
</html>
)=====";

const char Preview[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
</head>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 120px;
  font-size: 20px;
}
</style>
<body>
<p style="font-family:Arial;font-size:30px"><b>Previsualizaci&oacute;n</b><br><br></p>
<img src="/bitmap.bmp" width="myWI" height="myHE">
<br><br><br>&nbsp;&nbsp;
<input type="button" style="font-size:20px" value="Aceptar" onclick="history.go(-1)">
</body>
</html>
)=====";

const char badParam[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
</head>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 120px;
  font-size: 20px;
}
</style>
<body>
<p style="font-family:Arial;font-size:30px"><b>Chrono Labeler</b><br></p>

<br><p style="font-size:20px">Par&aacute;metro inv&aacute;lido: BADPAR<br><br>
<br>&nbsp;&nbsp;<input type="button" value="Aceptar" onclick="window.location.replace('/configPrn')">
</body>
</html>
)=====";
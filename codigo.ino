#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Pines de los relés
#define RELAY1 D1
#define RELAY2 D2

// Servidor web
ESP8266WebServer server(80);

// Variables de riego
int wateringDuration[7] = {0, 0, 0, 0, 0, 0, 0};  // Duración por día de la semana
int wateringInterval[7] = {0, 0, 0, 0, 0, 0, 0};  // Intervalo por día de la semana
int startHour[7] = {0, 0, 0, 0, 0, 0, 0};         // Hora de inicio por día
int startMinute[7] = {0, 0, 0, 0, 0, 0, 0};       // Minuto de inicio por día
int endHour[7] = {0, 0, 0, 0, 0, 0, 0};           // Hora de fin por día
int endMinute[7] = {0, 0, 0, 0, 0, 0, 0};         // Minuto de fin por día
bool daysOfWeek[7] = {false, false, false, false, false, false, false};  // Activo por día
bool manualMode = false;  // Modo manual
bool valve1State = false;  // Estado de la válvula 1
bool valve2State = false;  // Estado de la válvula 2

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 para Argentina

// Función para manejar la edición de un día
void handleEditDay() {
  int day = server.arg("day").toInt();
  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>TWISTED TRANSISTOR - Editar Día</title><style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f4f7; color: #333; }";
  html += ".container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 10px; }";
  html += ".config-item { padding: 10px; margin: 10px 0; background-color: #e0f7fa; text-align: center; border-radius: 5px; }";
  html += "button { margin: 10px; padding: 10px; background-color: #00796b; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += "</style></head><body><div class='container'><h1>Editar Día: " + String(day) + "</h1>";

  // Formulario para editar los parámetros del día seleccionado
  html += "<form action='/saveday' method='post'>";
  html += "<input type='hidden' name='day' value='" + String(day) + "'>";
  html += "<div class='config-item'>Duración de riego (minutos): <input type='number' name='duration' value='" + String(wateringDuration[day]) + "'></div>";
  html += "<div class='config-item'>Intervalo de riego (minutos): <input type='number' name='interval' value='" + String(wateringInterval[day]) + "'></div>";
  html += "<div class='config-item'>Hora de inicio: <input type='number' name='startHour' value='" + String(startHour[day]) + "' min='0' max='23'> : <input type='number' name='startMinute' value='" + String(startMinute[day]) + "' min='0' max='59'></div>";
  html += "<div class='config-item'>Hora de fin: <input type='number' name='endHour' value='" + String(endHour[day]) + "' min='0' max='23'> : <input type='number' name='endMinute' value='" + String(endMinute[day]) + "' min='0' max='59'></div>";
  html += "<div class='config-item'><label><input type='checkbox' name='active'" + String(daysOfWeek[day] ? " checked" : "") + "> Activar este día</label></div>";
  html += "<div><button type='submit'>Guardar</button></div>";
  html += "</form>";
  html += "<button onclick=\"location.href='/'\">Volver</button>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// Función para guardar los parámetros del día
void handleSaveDay() {
  int day = server.arg("day").toInt();
  
  // Validar que las horas y minutos sean válidos
  int startH = server.arg("startHour").toInt();
  int startM = server.arg("startMinute").toInt();
  int endH = server.arg("endHour").toInt();
  int endM = server.arg("endMinute").toInt();
  
  if (startM >= 60 || endM >= 60 || startH >= 24 || endH >= 24) {
    server.send(400, "text/html", "<html><body><h1>Error: Formato incorrecto de hora o minutos</h1></body></html>");
    return;
  }
  
  // Guardar los valores
  wateringDuration[day] = server.arg("duration").toInt();
  wateringInterval[day] = server.arg("interval").toInt();
  startHour[day] = startH;
  startMinute[day] = startM;
  endHour[day] = endH;
  endMinute[day] = endM;
  daysOfWeek[day] = server.hasArg("active");
  showConfig();
}

// Función para manejar el modo manual
void handleManualMode(String action) {
  manualMode = (action == "on");
  showConfig();
}

// Función para controlar las válvulas
void handleValveControl(String valve, String action) {
  if (valve == "valve1") {
    valve1State = (action == "on");
    digitalWrite(RELAY1, valve1State ? LOW : HIGH);  // Controlar el relé
  } else if (valve == "valve2") {
    valve2State = (action == "on");
    digitalWrite(RELAY2, valve2State ? LOW : HIGH);  // Controlar el relé
  }
  showConfig();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Válvulas cerradas por defecto
  digitalWrite(RELAY2, HIGH);

  WiFiManager wifiManager;
  wifiManager.autoConnect("TWISTED_TRANSISTOR_AP");

  timeClient.begin();
  timeClient.update();
  // Ruta para obtener la hora actual
server.on("/getTimeAndStates", []() {
  String response;

  // Crear JSON para actualizar múltiples elementos
  response += "{";
  
  // Hora actual
  response += "\"time\":\"Hora actual: " + timeClient.getFormattedTime() + "<br>Día actual: ";
  String daysOfWeekString[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  int currentDay = timeClient.getDay();
  currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)
  response += daysOfWeekString[currentDay] + "<br>\",";

  // Estado de las válvulas
  response += "\"valves\":\"" + getValveStateHtml() + "\",";

  // Estado de los días de la semana (colores)
  response += "\"days\":\"";
  String days[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  for (int i = 0; i < 7; i++) {
    String dayClass = daysOfWeek[i] ? "day-button day-active" : "day-button day-inactive";
    response += "<button class='" + dayClass + "' onclick=\\\"location.href='/editday?day=" + String(i) + "'\\\">" + days[i] + "</button>";
  }
  response += "\"}";

  // Enviar la respuesta en formato JSON
  server.send(200, "application/json", response);
});
  server.on("/getTime", []() {
    String timeString = timeClient.getFormattedTime();
    server.send(200, "text/plain", timeString);
  });
   // Ruta para obtener el estado de las válvulas
  server.on("/getValveStates", []() {
    server.send(200, "text/html", getValveStateHtml());
  });
  // Configuración del servidor web
  server.on("/", showConfig);
  server.on("/editday", handleEditDay);
  server.on("/saveday", HTTP_POST, handleSaveDay);
  server.on("/manual/on", []() { handleManualMode("on"); });
  server.on("/manual/off", []() { handleManualMode("off"); });
  server.on("/valve1/on", []() { handleValveControl("valve1", "on"); });
  server.on("/valve1/off", []() { handleValveControl("valve1", "off"); });
  server.on("/valve2/on", []() { handleValveControl("valve2", "on"); });
  server.on("/valve2/off", []() { handleValveControl("valve2", "off"); });
  server.begin();
}void loop() {
  server.handleClient();
  timeClient.update();  // Actualizar la hora desde el servidor NTP

  // Si el modo manual está activo, no ejecutar el control automático
  if (manualMode) {
    // Aquí el control de válvulas está en manos del usuario, no hacer nada en automático
    return;
  }

  // Si no está en modo manual, ejecutar el control automático
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentDay = timeClient.getDay(); // 0 es Domingo, 6 es Sábado
  currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)

  // Verificar si el día de riego está activo
  if (daysOfWeek[currentDay]) {
    // Verificar si la hora actual está dentro del rango configurado
    if ((currentHour > startHour[currentDay] || (currentHour == startHour[currentDay] && currentMinute >= startMinute[currentDay])) &&
        (currentHour < endHour[currentDay] || (currentHour == endHour[currentDay] && currentMinute <= endMinute[currentDay]))) {
      if (!valve1State) {  // Si las válvulas no están ya abiertas
        Serial.println("Iniciando riego automático, abriendo válvulas...");
        digitalWrite(RELAY1, LOW);  // Abrir válvula 1
        digitalWrite(RELAY2, LOW);  // Abrir válvula 2
        valve1State = true;
        valve2State = true;
      }
    } else {
      if (valve1State) {  // Si las válvulas están abiertas y ya terminó el tiempo
        Serial.println("Finalizando riego automático, cerrando válvulas...");
        digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1
        digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2
        valve1State = false;
        valve2State = false;
      }
    }
  } else {
    // Cerrar las válvulas si el día no está activo
    if (valve1State) {
      Serial.println("Día no activado, cerrando válvulas...");
      digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1
      digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2
      valve1State = false;
      valve2State = false;
    }
  }
}
void showConfig() {
  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>TWISTED TRANSISTOR - Sistema de Riego</title><style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f4f7; color: #333; }";
  html += ".container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 10px; }";
  html += "button { margin: 10px; padding: 10px; background-color: #00796b; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".day-button { margin: 5px; padding: 10px; border-radius: 5px; color: white; cursor: pointer; }";
  html += ".day-active { background-color: green; } .day-inactive { background-color: red; }";
  html += ".config-item { padding: 10px; margin: 10px 0; background-color: #e0f7fa; text-align: center; border-radius: 5px; }";
  html += "</style>";

  html += "</head><body><div class='container'><h1>Sistema de Riego TWISTED TRANSISTOR</h1>";

  // Mostrar la hora y el estado de los días
  html += "<h2>Hora, Día Actual y Estado de los Días</h2>";
  html += "<div id='currentTime'>Hora actual: " + timeClient.getFormattedTime() + "<br>";

  // Mostrar el día de la semana
  String daysOfWeekString[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  int currentDay = timeClient.getDay();
  currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)
  html += "Día actual: " + daysOfWeekString[currentDay] + "<br></div>";

  // Mostrar el estado de las válvulas
  html += "<div id='valveStates'>" + getValveStateHtml() + "</div>";

  // Días de la semana con sus colores y opciones
  html += "<div id='dayStates'>";
  String days[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  for (int i = 0; i < 7; i++) {
    String dayClass = daysOfWeek[i] ? "day-button day-active" : "day-button day-inactive";
    html += "<button class='" + dayClass + "' onclick=\"location.href='/editday?day=" + String(i) + "'\">" + days[i] + "</button>";
  }
  html += "</div>";

  // Botón para activar/desactivar modo manual
  html += "<h2>Modo Manual</h2>";
  html += "<div><button onclick=\"location.href='/manual/" + String(manualMode ? "off" : "on") + "'\">" + (manualMode ? "Desactivar Manual" : "Activar Manual") + "</button></div>";

  // Botones para abrir/cerrar válvulas en modo manual
  if (manualMode) {
    html += "<h2>Control de Válvulas (Modo Manual)</h2>";
    html += "<button onclick=\"location.href='/valve1/" + String(valve1State ? "off" : "on") + "'\">" + (valve1State ? "Cerrar Válvula 1" : "Abrir Válvula 1") + "</button>";
    html += "<button onclick=\"location.href='/valve2/" + String(valve2State ? "off" : "on") + "'\">" + (valve2State ? "Cerrar Válvula 2" : "Abrir Válvula 2") + "</button>";
  }

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}








// Función para obtener el estado de las válvulas y devolverlo en HTML
String getValveStateHtml() {
  String valve1Status = valve1State ? "<span style='color:blue;'>Abierta</span>" : "<span style='color:red;'>Cerrada</span>";
  String valve2Status = valve2State ? "<span style='color:blue;'>Abierta</span>" : "<span style='color:red;'>Cerrada</span>";
  return "<div><strong>Válvula 1:</strong> " + valve1Status + " | <strong>Válvula 2:</strong> " + valve2Status + "</div>";
}


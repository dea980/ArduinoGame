#include <SPI.h>
#include <Ethernet.h>


// MAC address for the Ethernet shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };


// IP address for the server
IPAddress ip(192, 168, 8, 122);


// Create an Ethernet Server object on port 80
EthernetServer server(80);


// Message buffer (basic, limited to 10 messages)
#define MAX_MESSAGES 10
String chatLog[MAX_MESSAGES];
int messageCount = 0;


void setup() {
  // Initialize serial communication
  Serial.begin(115200);


  // Initialize the Ethernet shield with the MAC address and IP address
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // No DHCP, use static IP
    Ethernet.begin(mac, ip);
  } else {
    // Use the DHCP-assigned IP address
    ip = Ethernet.localIP();
    Serial.print("Assigned IP: ");
    Serial.println(ip);
  }


  // Give the Ethernet shield a second to initialize
  delay(1000);


  // Start listening for clients
  server.begin();
  Serial.print("Server started at ");
  Serial.println(ip);
}


void loop() {
  // Check if a client has connected
  EthernetClient client = server.available();


  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        Serial.write(c);


        if (c == '\n' && currentLine.length() == 0) {
          // End of headers
          if (request.startsWith("POST")) {
            int contentLengthStart = request.indexOf("Content-Length: ");
            if (contentLengthStart != -1) {
              // Get content length from headers
              int contentLengthEnd = request.indexOf("\r\n", contentLengthStart);
              int contentLength = request.substring(contentLengthStart + 16, contentLengthEnd).toInt();
              Serial.print("Content-Length: ");
              Serial.println(contentLength);


              // Read and process POST data
              while (client.available() < contentLength) {
                delay(1);
              }
              String postData = "";
              for (int i = 0; i < contentLength; ++i) {
                postData += (char)client.read();
              }
              Serial.print("POST Data: ");
              Serial.println(postData);


              processPostRequest(postData);
            }
          } else if (request.startsWith("GET /messages")) {
            sendMessages(client);
          } else {
            sendWebPage(client);
          }
          break;
        } else if (c == '\n') {
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }


    // Close the connection
    client.stop();
    Serial.println("Client disconnected");
  }
}


void processPostRequest(String data) {
  if (data.length() > 0) {
    int usernameStart = data.indexOf("username=");
    int messageStart = data.indexOf("message=");
    if (usernameStart != -1 && messageStart != -1) {
      String username = urlDecode(data.substring(usernameStart + 9, data.indexOf('&', usernameStart)));
      String message = urlDecode(data.substring(messageStart + 8));
      if (message.length() == 0) {
        return; // Prevent empty messages
      }
      String chatMessage = username + ": " + message;


      // Add message to chat log
      if (messageCount < MAX_MESSAGES) {
        chatLog[messageCount] = chatMessage;
        messageCount++;
      } else {
        // Shift messages to make room
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
          chatLog[i] = chatLog[i + 1];
        }
        chatLog[MAX_MESSAGES - 1] = chatMessage;
      }
      Serial.println("Stored message: " + chatMessage);
    }
  }
}


String urlDecode(String input) {
  String output = "";
  char a, b;
  int i = 0;
  while (i < input.length()) {
    if (input[i] == '%') {
      a = input[i + 1];
      b = input[i + 2];
      if (a >= 'a') a -= 32; // Convert 'a'-'f' to 'A'-'F'
      if (a > '9') a -= 7;   // Convert 'A'-'F' to 10-15
      a -= '0';
      if (b >= 'a') b -= 32; // Convert 'a'-'f' to 'A'-'F'
      if (b > '9') b -= 7;   // Convert 'A'-'F' to 10-15
      b -= '0';
      output += char(a * 16 + b);
      i += 3;
    } else if (input[i] == '+') {
      output += ' ';
      i++;
    } else {
      output += input[i];
      i++;
    }
  }
  return output;
}


void sendWebPage(EthernetClient& client) {
  // Send a standard HTTP response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close"); // The connection will be closed after the response
  client.println();


  // Web page content
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head><meta charset=\"UTF-8\">");
  client.println("<title>Arduino Chatroom</title>");
  client.println("<style>");
  client.println("body { background-color: black; color: white; font-family: Arial, sans-serif; display: flex; flex-direction: column; justify-content: flex-end; height: 100vh; margin: 0; overflow: hidden; }");
  client.println("#message { width: 100%; height: 100px; }");
  client.println("#chatLog { flex: 1; overflow-y: auto; padding: 10px; box-sizing: border-box; }");
  client.println("</style>");
  client.println("<script>");
  client.println("document.addEventListener('DOMContentLoaded', function() {");
  client.println("  var username = localStorage.getItem('username');");
  client.println("  if (!username) {");
  client.println("    username = prompt('Enter your username:') || 'User';");
  client.println("    localStorage.setItem('username', username);");
  client.println("  }");
  client.println("  document.getElementById('username').value = localStorage.getItem('username');");


  client.println("  var color = localStorage.getItem('color');");
  client.println("  if (!color) {");
  client.println("    color = getRandomColor();");
  client.println("    localStorage.setItem('color', color);");
  client.println("  }");
  client.println("  setRandomUsernameColor(username, color);");
  client.println("});");


  client.println("function sendMessage() {");
  client.println("  var messageInput = document.getElementById('message');");
  client.println("  if (messageInput.value.trim() === '') return;"); // Prevent empty message
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  var username = document.getElementById('username').value;");
  client.println("  var message = messageInput.value;");
  client.println("  xhr.open('POST', '/', true);");
  client.println("  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');");
  client.println("  xhr.send('username=' + encodeURIComponent(username) + '&message=' + encodeURIComponent(message));");
  client.println("  messageInput.value = '';");
  client.println("}");


  client.println("function fetchMessages() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('GET', '/messages', true);");
  client.println("  xhr.onreadystatechange = function() {");
  client.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
  client.println("      var chatLog = document.getElementById('chatLog');");
  client.println("      chatLog.innerHTML = xhr.responseText.replace(/\\n/g, '<br>');");
  client.println("      chatLog.scrollTop = chatLog.scrollHeight; // Scroll to bottom");
  client.println("      var username = localStorage.getItem('username');");
  client.println("      var color = localStorage.getItem('color');");
  client.println("      setRandomUsernameColor(username, color);");
  client.println("    }");
  client.println("  };");
  client.println("  xhr.send();");
  client.println("}");


  client.println("document.addEventListener('keypress', function (e) {");
  client.println("  if (e.key === 'Enter') {");
  client.println("    e.preventDefault();");
  client.println("    sendMessage();");
  client.println("  }");
  client.println("});");


  client.println("function getRandomColor() {");
  client.println("  var letters = '0123456789ABCDEF';");
  client.println("  var color = '#';");
  client.println("  for (var i = 0; i < 6; i++) {");
  client.println("    color += letters[Math.floor(Math.random() * 16)];");
  client.println("  }");
  client.println("  return color;");
  client.println("}");


  client.println("function setRandomUsernameColor(username, color) {");
  client.println("  var chatLog = document.getElementById('chatLog').innerHTML;");
  client.println("  var regex = new RegExp(username + ':', 'g');");
  client.println("  document.getElementById('chatLog').innerHTML = chatLog.replace(regex, '<span style=\"color:' + color + '\">' + username + ':</span>');");
  client.println("}");


  client.println("setInterval(fetchMessages, 1000);");


  client.println("</script>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div id='chatLog'>");


  // Display the initial chat log
  for (int i = 0; i < messageCount; i++) {
    client.print("<div>");
    client.print(chatLog[i]);
    client.println("</div>");
  }


  client.println("</div>");
  client.println("<input type='hidden' id='username'><br>");
  client.println("<textarea id='message' placeholder='Enter your message here...'></textarea><br>");
  client.println("<button onclick='sendMessage()'>Send</button>");
  client.println("</body>");
  client.println("</html>");
}


void sendMessages(EthernetClient& client) {
  // Send a standard HTTP response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain; charset=UTF-8");
  client.println("Connection: close"); // The connection will be closed after the response
  client.println();


  // Display chat log
  for (int i = 0; i < messageCount; i++) {
    client.println(chatLog[i]);
  }
}




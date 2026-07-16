#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"

ESP8266WebServer server(80);

// Định nghĩa chân cho 2 cảm biến
const int TRIG_PIN3 = D1;    // Cảm biến 3 - Góc dưới (GPIO13)
const int ECHO_PIN3 = D2;    // GPIO15
const int TRIG_PIN4 = D5;    // Cảm biến 4 - Góc trái (GPIO14)
const int ECHO_PIN4 = D6;    // GPIO12

// Thông số buzzer 5VDC Active Buzzer với transistor NPN
const int BUZZER_PIN = D0;   // Buzzer control via NPN transistor on D0 (GPIO16)
const int BUZZER_RESONANT_FREQ = 2300; // Tần số cộng hưởng 2300Hz
const int BUZZER_MIN_VOLTAGE = 3500; // 3.5V = 3500mV
const int BUZZER_MAX_VOLTAGE = 5500; // 5.5V = 5500mV
const int BUZZER_CURRENT = 25; // <25mA
const int BASE_RESISTOR = 1000; // 1kΩ base resistor cho transistor
// QUAN TRỌNG: ESP8266 3.3V KHÔNG ĐỦ → CẦN TRANSISTOR NPN!

// Thông số cảm biến HC-SR05
const int MAX_DISTANCE = 450;  // Khoảng cách tối đa (cm)
const int MIN_DISTANCE = 2;    // Khoảng cách tối thiểu (cm)
const int RELIABLE_MAX = 100;  // Khoảng cách tin cậy tối đa (cm)
const int SENSOR_TIMEOUT = 25000; // Timeout cho cảm biến (microseconds)
const int MEASUREMENT_INTERVAL = 50; // Thời gian giữa các lần đo (ms)

// Hằng số tính khoảng cách cho HC-SR05
const float SOUND_VELOCITY = 0.017; // Vận tốc âm thanh trong không khí ở 20°C (cm/microsecond)

// Các thông số cảnh báo và lọc nhiễu
const float WARNING_DISTANCE = 7.0;  // Khoảng cách cảnh báo (cm)
const float SAFE_DISTANCE = 3.0;     // Khoảng cách an toàn (cm)
const int BEEP_DURATION = 100;       // Thời gian beep (ms)
const int BEEP_INTERVAL = 200;       // Khoảng cách giữa các beep (ms)
// Thông số lọc nhiễu
const int FILTER_SIZE = 5;           // Số mẫu để lọc
const float MAX_DEVIATION = 10.0;    // Độ lệch tối đa cho phép giữa các mẫu (cm)
const float MAX_DISTANCE_CHANGE = 50.0; // Thay đổi khoảng cách tối đa cho phép (cm)
float lastValidMeasurements[2][FILTER_SIZE]; // Lưu các giá trị đo gần nhất
int measurementIndex[2] = {0, 0};    // Chỉ số cho mảng giá trị
unsigned long lastMeasurementTime[2] = {0, 0}; // Thời gian đo cuối cùng cho mỗi cảm biến

// Hàm lọc nhiễu bằng trung vị
float medianFilter(float values[], int size) {
    float temp[size];
    memcpy(temp, values, size * sizeof(float));
    
    // Sắp xếp mảng
    for(int i = 0; i < size-1; i++) {
        for(int j = i+1; j < size; j++) {
            if(temp[i] > temp[j]) {
                float t = temp[i];
                temp[i] = temp[j];
                temp[j] = t;
            }
        }
    }
    
    // Trả về giá trị trung vị
    return temp[size/2];
}

// Hàm kiểm tra độ ổn định của phép đo
bool isStableMeasurement(float newValue, float values[], int size) {
    if (size < 3) return true; // Cần ít nhất 3 mẫu để kiểm tra
    
    float sum = 0;
    float max_val = values[0];
    float min_val = values[0];
    
    for(int i = 0; i < size; i++) {
        sum += values[i];
        if(values[i] > max_val) max_val = values[i];
        if(values[i] < min_val) min_val = values[i];
    }
    
    // Kiểm tra độ lệch
    float deviation = max_val - min_val;
    return deviation <= MAX_DEVIATION;
}

// Hàm kiểm tra giá trị hợp lệ
bool isValidMeasurement(float distance, float lastDistance) {
    if (distance < MIN_DISTANCE || distance > MAX_DISTANCE) {
        return false;
    }
    
    // Nếu đây là lần đo đầu tiên
    if (lastDistance < 0) {
        return true;
    }
    
    // Kiểm tra sự thay đổi đột ngột
    float change = abs(distance - lastDistance);
    return change <= MAX_DISTANCE_CHANGE;
}

// Cấu trúc dữ liệu cho cảm biến
struct SensorData {
    float distance;
    bool isActive;
    unsigned long startTime;
    int state;
};

// Mảng lưu trữ dữ liệu cảm biến
SensorData sensors[2];
bool globalWarningState = false; // Trạng thái cảnh báo toàn cục

// Định nghĩa HTML page với radar display
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Radar Siêu Âm</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background: #f0f0f0;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #2196F3;
        }
        .radar-container {
            position: relative;
            width: 400px;
            height: 400px;
            margin: 20px auto;
            background: #001f3f;
            border-radius: 50%;
            overflow: hidden;
        }
        .radar-background {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            border: 2px solid rgba(0, 255, 0, 0.3);
        }
        .radar-circle {
            position: absolute;
            border-radius: 50%;
            border: 1px solid rgba(0, 255, 0, 0.3);
        }
        .radar-line {
            position: absolute;
            width: 50%;
            height: 1px;
            background: rgba(0, 255, 0, 0.3);
            transform-origin: left center;
        }
        .center-point {
            position: absolute;
            width: 6px;
            height: 6px;
            background: red;
            border-radius: 50%;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            z-index: 2;
        }
        .radar-sweep {
            position: absolute;
            width: 50%;
            height: 50%;
            top: 50%;
            left: 50%;
            background: linear-gradient(90deg, 
                rgba(255, 0, 0, 0.2) 0%,
                rgba(255, 0, 0, 0) 100%
            );
            transform-origin: 0 0;
            animation: sweep 4s infinite linear;
        }
        .buffer-ring {
            position: absolute;
            border-radius: 50%;
            transform: translate(-50%, -50%);
            animation: expand 2s ease-out forwards;
            opacity: 0;
        }
        .buffer-ring.safe {
            border: 2px solid rgba(0, 255, 0, 0.5);
        }
        .buffer-ring.danger {
            border: 2px solid rgba(255, 0, 0, 0.5);
        }
        .radar-sweep.safe {
            background: linear-gradient(90deg, 
                rgba(0, 255, 0, 0.2) 0%,
                rgba(0, 255, 0, 0) 100%
            );
        }
        .radar-sweep.danger {
            background: linear-gradient(90deg, 
                rgba(255, 0, 0, 0.2) 0%,
                rgba(255, 0, 0, 0) 100%
            );
        }
        .center-point.safe {
            background: #00ff00;
        }
        .center-point.danger {
            background: #ff0000;
        }
        .distance-label {
            position: absolute;
            color: #00ff00;
            font-weight: bold;
            font-size: 14px;
            text-shadow: 1px 1px 2px black;
            z-index: 3;
        }
        .status {
            margin: 20px 0;
            padding: 10px;
            border-radius: 5px;
            background: #4CAF50;
            color: white;
        }
        @keyframes sweep {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }
        @keyframes expand {
            0% {
                width: 10px;
                height: 10px;
                opacity: 1;
            }
            100% {
                width: 100%;
                height: 100%;
                opacity: 0;
            }
        }
        
        .distance-marker {
            position: absolute;
            width: 100%;
            height: 100%;
            border-radius: 50%;
            border: 1px dashed rgba(0, 255, 0, 0.2);
            pointer-events: none;
        }
        
        .sensor-point {
            position: absolute;
            width: 8px;
            height: 8px;
            background: #00ff00;
            border-radius: 50%;
            transform: translate(-50%, -50%);
            transition: all 0.3s ease;
            opacity: 0;
            pointer-events: none;
        }
        
        .sensor-point.active {
            opacity: 1;
            box-shadow: 0 0 10px #00ff00;
        }
        
        .sensor-point.danger {
            background: #ff0000;
            box-shadow: 0 0 10px #ff0000;
        }
        
        .sensor-label {
            position: absolute;
            color: #00ff00;
            font-size: 12px;
            transform: translate(-50%, -50%);
            pointer-events: none;
            opacity: 0;
            transition: all 0.3s ease;
        }
        
        .sensor-label.active {
            opacity: 1;
        }
        
        .sensor-label.danger {
            color: #ff0000;
        }
        
        .sensor-distance {
            position: absolute;
            color: #00ff00;
            font-size: 14px;
            font-weight: bold;
            transform: translate(-50%, -50%);
            pointer-events: none;
            opacity: 0;
            transition: all 0.3s ease;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.8);
        }
        
        .sensor-distance.active {
            opacity: 1;
        }
        
        .sensor-distance.danger {
            color: #ff0000;
        }
        
        #status {
            margin: 20px 0;
            padding: 10px;
            border-radius: 5px;
            background: #4CAF50;
            color: white;
            transition: all 0.3s ease;
        }
        
        #status.danger {
            background: #f44336;
        }

        .buzzer-status {
            position: fixed;
            top: 10px;
            right: 10px;
            padding: 10px 20px;
            border-radius: 5px;
            font-weight: bold;
            z-index: 1000;
            transition: all 0.3s ease;
            background: #4CAF50;
            color: white;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        }
        
        .buzzer-status.active {
            background: #f44336;
            animation: blink 1s infinite;
        }
        
        @keyframes blink {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }
    </style>
</head>
<body>
    <div class="buzzer-status" id="buzzerStatus">Trạng thái: An toàn</div>
    <div class="container">
        <h1>Radar Siêu Âm</h1>
        <div class="radar-container" id="radar">
            <div class="radar-background"></div>
            
            <!-- Vòng tròn đồng tâm -->
            <div class="radar-circle" style="width: 75%; height: 75%; top: 12.5%; left: 12.5%;"></div>
            <div class="radar-circle" style="width: 50%; height: 50%; top: 25%; left: 25%;"></div>
            <div class="radar-circle" style="width: 25%; height: 25%; top: 37.5%; left: 37.5%;"></div>
            
            <!-- Distance markers without labels -->
            <div id="distance-markers"></div>
            
            <!-- Đường thẳng -->
            <div class="radar-line" style="transform: rotate(0deg)"></div>
            <div class="radar-line" style="transform: rotate(90deg)"></div>
            <div class="radar-line" style="transform: rotate(180deg)"></div>
            <div class="radar-line" style="transform: rotate(270deg)"></div>
            
            <!-- Sensor points and labels -->
            <div class="sensor-point" id="sensor1" style="top: 10%; left: 50%"></div>
            <div class="sensor-point" id="sensor2" style="top: 50%; left: 90%"></div>
            <div class="sensor-point" id="sensor3" style="top: 90%; left: 50%"></div>
            <div class="sensor-point" id="sensor4" style="top: 50%; left: 10%"></div>
            
            <div class="sensor-label" id="label1" style="top: 5%; left: 50%">S1</div>
            <div class="sensor-label" id="label2" style="top: 50%; left: 95%">S2</div>
            <div class="sensor-label" id="label3" style="top: 95%; left: 50%">S3</div>
            <div class="sensor-label" id="label4" style="top: 50%; left: 3%">S4</div>
            
            <div class="sensor-distance" id="distance1" style="top: 15%; left: 50%"></div>
            <div class="sensor-distance" id="distance2" style="top: 50%; left: 85%"></div>
            <div class="sensor-distance" id="distance3" style="top: 85%; left: 50%"></div>
            <div class="sensor-distance" id="distance4" style="top: 50%; left: 18%"></div>
            
            <div class="center-point safe" id="center-point"></div>
            <div class="radar-sweep safe" id="radar-sweep"></div>
        </div>
        <div id="status">Đang quét...</div>
    </div>

    <script>
        const MAX_DISTANCE = 400; // cm
        const RADAR_SIZE = 400; // px
        const SAFE_DISTANCE = 300; // 3m = 300cm
        
        // Tạo distance markers không có nhãn
        function createDistanceMarkers() {
            const markers = document.getElementById('distance-markers');
            const distances = [100, 200, 300, 400]; // cm
            
            distances.forEach(distance => {
                const percentage = (distance / MAX_DISTANCE) * 100;
                const size = percentage + '%';
                
                const marker = document.createElement('div');
                marker.className = 'distance-marker';
                marker.style.width = size;
                marker.style.height = size;
                marker.style.top = (100 - percentage) / 2 + '%';
                marker.style.left = (100 - percentage) / 2 + '%';
                
                markers.appendChild(marker);
            });
        }
        
        function updateSensor(id, data) {
            const sensor = document.getElementById('sensor' + id);
            const label = document.getElementById('label' + id);
            const distance = document.getElementById('distance' + id);
            
            if (data.distance > 0) {
                // Hiển thị và cập nhật trạng thái
                sensor.classList.add('active');
                label.classList.add('active');
                distance.classList.add('active');
                
                // Cập nhật trạng thái cảnh báo
                if (data.warning) {
                    sensor.classList.add('danger');
                    label.classList.add('danger');
                    distance.classList.add('danger');
                } else {
                    sensor.classList.remove('danger');
                    label.classList.remove('danger');
                    distance.classList.remove('danger');
                }
                
                // Hiển thị khoảng cách
                distance.textContent = data.distance.toFixed(1) + 'cm';
            } else {
                // Ẩn khi không có dữ liệu
                sensor.classList.remove('active', 'danger');
                label.classList.remove('active', 'danger');
                distance.classList.remove('active', 'danger');
            }
        }
        
        let buzzerActive = false;
        let lastBuzzerState = false;
        
        function updateBuzzerStatus(isActive) {
            const buzzerStatus = document.getElementById('buzzerStatus');
            if (isActive !== lastBuzzerState) {
                buzzerStatus.textContent = isActive ? 'Trạng thái: Không an toàn!' : 'Trạng thái: An toàn';
                buzzerStatus.className = 'buzzer-status' + (isActive ? ' active' : '');
                lastBuzzerState = isActive;
            }
        }
        
        // Cập nhật hàm updateStatus để thêm xử lý trạng thái còi
        function updateStatus(data) {
            const status = document.getElementById('status');
            const warnings = data.filter(s => s.warning && s.distance > 0);
            
            // Cập nhật trạng thái còi
            updateBuzzerStatus(warnings.length > 0);
            
            if (warnings.length > 0) {
                status.classList.add('danger');
                const messages = warnings.map((s, i) => {
                    const index = data.indexOf(s) + 3;
                    return `S${index}: ${s.distance.toFixed(1)}cm`;
                });
                status.textContent = '⚠️ Cảnh báo: ' + messages.join(' | ');
            } else {
                status.classList.remove('danger');
                status.textContent = 'Đang quét...';
            }
        }
        
        // Cập nhật dữ liệu từ server
        setInterval(function() {
            fetch('/laydulieu')
                .then(response => response.json())
                .then(data => {
                    console.log('Data received:', data);
                    data.forEach((sensor, index) => {
                        updateSensor(index + 3, sensor);
                    });
                    updateStatus(data);
                })
                .catch(error => console.error('Error:', error));
        }, 100);
    </script>
</body>
</html>
)=====";

// Hàm test buzzer với nhiều phương pháp khác nhau
void testBuzzerMethods() {
    Serial.println("🧪 === TEST BUZZER 5VDC ACTIVE ===");
    
    // Method 1: digitalWrite HIGH (Phương pháp chính cho Active Buzzer)
    Serial.println("🔍 Test 1: digitalWrite HIGH (Active Buzzer)");
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("   → D0 HIGH: 3 giây...");
    Serial.println("   → Nếu có transistor: Buzzer sẽ kêu");
    Serial.println("   → Nếu chưa có transistor: Vẫn im lặng");
    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("   → D0 OFF");
    delay(1000);
    
    // Method 2: PWM với duty cycle cao (Thử nghiệm)
    Serial.println("🔍 Test 2: PWM HIGH duty cycle");
    analogWrite(BUZZER_PIN, 1023); // Max PWM
    Serial.println("   → PWM 100%: 2 giây...");
    delay(2000);
    analogWrite(BUZZER_PIN, 0);
    Serial.println("   → PWM OFF");
    delay(1000);
    
    // Method 3: Test với pattern ON/OFF
    Serial.println("🔍 Test 3: Pattern ON/OFF (5 lần)");
    for(int i = 0; i < 5; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
        Serial.print(".");
    }
    Serial.println("\n   ✅ Pattern test hoàn tất");
    
    Serial.println("✅ Tất cả test hoàn tất!\n");
}

// Hàm kiểm tra và test buzzer
void testBuzzer() {
    Serial.println("\n🔧 === KIỂM TRA BUZZER 5VDC ACTIVE ===");
    Serial.println("📍 Vị trí: Pin D8 (GPIO15)");
    
    // Test đa phương pháp
    testBuzzerMethods();
    
    Serial.print("🔊 Test cuối: digitalWrite liên tục 2 giây... ");
    
    // Test buzzer kêu liên tục trong 2 giây với digitalWrite
    unsigned long startTime = millis();
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.print("HIGH ");
    
    while (millis() - startTime < 2000) {
        // Kêu liên tục trong 2 giây
        delay(100);
        Serial.print(".");
    }
    
    // Tắt buzzer sau khi test
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println(" ✅ HOÀN THÀNH!");
    
    // Kiểm tra phản hồi từ người dùng qua Serial
    Serial.println("📋 Kết quả test:");
    Serial.println("   - Test 1 có tiếng → Active Buzzer");
    Serial.println("   - Test 2 hoặc 3 có tiếng → Passive Buzzer");
    Serial.println("   - Không có tiếng nào → Kiểm tra kết nối");
    Serial.println("🚀 Chương trình sẽ bắt đầu sau 1 giây...\n");
    
    delay(1000);
}

// Hàm kiểm tra kết nối buzzer bằng cách đọc trạng thái pin
bool checkBuzzerConnection() {
    Serial.println("🔍 Kiểm tra kết nối buzzer pin D0...");
    Serial.print("GPIO số: ");
    Serial.println(BUZZER_PIN);
    
    // Set pin D0 làm output và test
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(10);
    
    // Đọc trạng thái pin
    bool pinState = digitalRead(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    
    Serial.print("Trạng thái pin test: ");
    Serial.println(pinState ? "HIGH" : "LOW");
    
    if (pinState == HIGH) {
        Serial.println("✅ Pin D0 phản hồi bình thường");
        Serial.println("⚠️ Nhưng cần transistor NPN cho buzzer 5V");
        return true;
    } else {
        Serial.println("⚠️ Cảnh báo: Pin D0 có thể có vấn đề");
        return false;
    }
}

// Hàm test điện áp output của pin D0
void testPinVoltage() {
    Serial.println("🔬 === TEST ĐIỆN ÁP PIN D0 ===");
    
    // Test digitalWrite HIGH
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    Serial.println("📏 Pin D0 = HIGH:");
    Serial.println("   - Dùng multimeter đo điện áp D0 → GND");
    Serial.println("   - Nên đo được ~3.3V");
    delay(2000); // 2 giây để đo
    
    // Test digitalWrite LOW  
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    Serial.println("📏 Pin D0 = LOW:");
    Serial.println("   - Dùng multimeter đo điện áp D0 → GND");
    Serial.println("   - Nên đo được ~0V");
    delay(2000); // 2 giây để đo
    
    Serial.println("✅ Test điện áp hoàn tất");
    Serial.println("");
    Serial.println("🛒 DANH SÁCH MUA LINH KIỆN:");
    Serial.println("   1. Transistor NPN BC547 hoặc 2N2222 (1 cái)");
    Serial.println("   2. Điện trở 1kΩ (1 cái)");
    Serial.println("   3. Breadboard nhỏ hoặc PCB");
    Serial.println("   4. Dây nối");
    Serial.println("");
}

void handleRoot() {
  String s = MAIN_page;
  server.send(200, "text/html", s);
}

// Hàm đo khoảng cách cho một cảm biến - Phiên bản cải tiến
float khoang_cach(int trigPin, int echoPin, int sensorIndex) {
    // Reset trigger pin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    
    // Gửi xung trigger 10us
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    // Đọc thời gian phản hồi
    unsigned long duration = pulseIn(echoPin, HIGH, SENSOR_TIMEOUT);
    
    if (duration == 0) {
        return -1;
    }
    
    // Tính khoảng cách
    float distance = (duration * SOUND_VELOCITY);
    
    // Kiểm tra giới hạn khoảng cách
    if (distance >= MIN_DISTANCE && distance <= MAX_DISTANCE) {
        // Lưu giá trị đo mới
        lastValidMeasurements[sensorIndex][measurementIndex[sensorIndex]] = distance;
        measurementIndex[sensorIndex] = (measurementIndex[sensorIndex] + 1) % FILTER_SIZE;
        
        // Kiểm tra độ ổn định
        if (isStableMeasurement(distance, lastValidMeasurements[sensorIndex], FILTER_SIZE)) {
            // Áp dụng bộ lọc trung vị nếu đo ổn định
            float filteredDistance = medianFilter(lastValidMeasurements[sensorIndex], FILTER_SIZE);
            
            // Debug thông tin
            Serial.print("Sensor ");
            Serial.print(sensorIndex + 3);
            Serial.print(" - Raw: ");
            Serial.print(distance);
            Serial.print("cm, Filtered: ");
            Serial.print(filteredDistance);
            Serial.println("cm");
            
            return filteredDistance;
        } else {
            Serial.print("Sensor ");
            Serial.print(sensorIndex + 3);
            Serial.println(" - Unstable measurement detected!");
            return -1;
        }
    }
    
    return -1;
}

void handleGetData() {
    float distances[2] = {-1, -1};
    static unsigned long lastLogTime = 0;
    
    // Đọc cảm biến 3
    distances[0] = khoang_cach(TRIG_PIN3, ECHO_PIN3, 0);
    lastMeasurementTime[0] = millis(); // Cập nhật thời gian đo
    delay(10);
    // Đọc cảm biến 4
    distances[1] = khoang_cach(TRIG_PIN4, ECHO_PIN4, 1);
    lastMeasurementTime[1] = millis(); // Cập nhật thời gian đo
    
    // Cập nhật dữ liệu cảm biến
    for (int i = 0; i < 2; i++) {
        if (distances[i] > 0) {
            sensors[i].distance = distances[i];
            sensors[i].isActive = true;
        } else {
            sensors[i].isActive = false;
        }
    }
    
    // In log khoảng cách mỗi 500ms
    if (millis() - lastLogTime >= 500) {
        Serial.println("\n=== DISTANCE LOG ===");
        for (int i = 0; i < 2; i++) {
            Serial.print("Sensor ");
            Serial.print(i + 3);
            Serial.print(": ");
            if (distances[i] > 0) {
                Serial.print(distances[i], 1);
                Serial.print(" cm | Status: ");
                if (distances[i] < SAFE_DISTANCE) {
                    Serial.println("QUÁ GẦN!");
                } else if (distances[i] <= WARNING_DISTANCE) {
                    Serial.println("VÙNG CẢNH BÁO!");
                } else {
                    Serial.println("An toàn");
                }
            } else {
                Serial.println("Không phát hiện");
            }
        }
        Serial.println("==================");
        lastLogTime = millis();
    }
    
    // Kiểm tra cảnh báo
    bool hasWarning = false;
    String json = "[";
    
    for (int i = 0; i < 2; i++) {
        if (i > 0) json += ",";
        json += "{";
        
        if (distances[i] > 0) {
            json += "\"distance\":" + String(distances[i], 1);
            bool isWarning = (distances[i] <= WARNING_DISTANCE);
            json += ",\"warning\":" + String(isWarning ? "true" : "false");
            
            if (isWarning) {
                hasWarning = true;
            }
        } else {
            json += "\"distance\":-1,\"warning\":false";
        }
        json += "}";
    }
    json += "]";
    
    // Cập nhật trạng thái cảnh báo toàn cục
    globalWarningState = hasWarning;
    
    // Debug buzzer state với thông tin chi tiết
    if (hasWarning) {
        Serial.println("=== CẢNH BÁO PHÁT HIỆN ===");
        Serial.println("Global Warning State: TRUE");
        
        // Đếm số cảm biến cảnh báo
        int warningCount = 0;
        for (int i = 0; i < 2; i++) {
            if (distances[i] > 0 && distances[i] <= WARNING_DISTANCE) {
                warningCount++;
            }
        }
        Serial.print("Số cảm biến cảnh báo: ");
        Serial.println(warningCount);
        
        for (int i = 0; i < 2; i++) {
            if (distances[i] > 0 && distances[i] <= WARNING_DISTANCE) {
                Serial.print("Sensor ");
                Serial.print(i + 3);
                Serial.print(": ");
                Serial.print(distances[i], 1);
                Serial.print("cm <= ");
                Serial.print(WARNING_DISTANCE);
                Serial.println("cm (CẢNH BÁO!)");
            }
        }
        Serial.println("Buzzer sẽ được kích hoạt trong loop()");
        Serial.println("================================");
    } else {
        // Chỉ log khi chuyển từ warning sang safe
        static bool lastWarningState = false;
        if (lastWarningState) {
            Serial.println("✅ Trở về trạng thái AN TOÀN");
            Serial.println("Global Warning State: FALSE");
        }
        lastWarningState = hasWarning;
    }
    
    server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n🚀 === KHỞI ĐỘNG HỆ THỐNG RADAR SIÊU ÂM ===");
  
  // Setup pins
  pinMode(TRIG_PIN3, OUTPUT);
  pinMode(ECHO_PIN3, INPUT);
  pinMode(TRIG_PIN4, OUTPUT);
  pinMode(ECHO_PIN4, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Reset pins
  digitalWrite(TRIG_PIN3, LOW);
  digitalWrite(TRIG_PIN4, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("📌 Thiết lập chân I/O hoàn tất");
  
  // Test buzzer trước khi bắt đầu
  Serial.println("🔧 Kiểm tra buzzer...");
  if (checkBuzzerConnection()) {
      testBuzzer();
      // Thêm test điện áp để debug
      testPinVoltage();
  } else {
      Serial.println("⚠️ CẢNH BÁO: Buzzer có thể không hoạt động!");
      Serial.println("   Kiểm tra kết nối pin D8");
  }
  
  // Kết nối WiFi
  Serial.println("📶 Kết nối WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin("YOUR_ID_WIFI_HERE", "YOUR_PASSWORD_WIFI_HERE");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ Đã kết nối WiFi!");
  Serial.print("🌐 IP Address: http://");
  Serial.println(WiFi.localIP());
  
  // Khởi tạo web server
  server.on("/", handleRoot);
  server.on("/laydulieu", handleGetData);
  server.begin();
  Serial.println("🔗 Web Server đã sẵn sàng");
  
  // Hiển thị thông báo và chờ 2 giây trước khi bắt đầu đo
  Serial.println("📝 Ghi nhận địa chỉ IP trong 2 giây...");
  Serial.println("⏰ Sau 2 giây sẽ bắt đầu đo khoảng cách");
  Serial.print("⏳ Đếm ngược: 2");
  delay(1000);
  Serial.print("... 1");
  delay(1000);
  Serial.println("... 0");
  
  Serial.println("🎯 Hệ thống radar đã khởi động hoàn tất!");
  Serial.println("🚀 BẮT ĐẦU ĐO KHOẢNG CÁCH...\n");
}

void loop() {
    static bool lastBuzzerState = false;
    
    unsigned long currentMillis = millis();
    
    // Đọc cảm biến 3 với interval
    if (currentMillis - lastMeasurementTime[0] >= MEASUREMENT_INTERVAL) {
        khoang_cach(TRIG_PIN3, ECHO_PIN3, 0); // Đọc để cập nhật bộ lọc
        lastMeasurementTime[0] = currentMillis;
    }
    
    // Đọc cảm biến 4 với interval
    if (currentMillis - lastMeasurementTime[1] >= MEASUREMENT_INTERVAL) {
        khoang_cach(TRIG_PIN4, ECHO_PIN4, 1); // Đọc để cập nhật bộ lọc
        lastMeasurementTime[1] = currentMillis;
    }
    
    // Xử lý buzzer - KÊU LIÊN TỤC khi có cảnh báo (Kiểm tra trạng thái D0)
    if (globalWarningState) {
        if (!lastBuzzerState) {
            Serial.println("🚨 BUZZER: BẮT ĐẦU KÊU LIÊN TỤC - CẢNH BÁO!");
            
            // Đọc trạng thái D0 trước khi set HIGH
            int stateBefore = digitalRead(BUZZER_PIN);
            Serial.print("📏 D0 trước khi set HIGH: ");
            Serial.println(stateBefore == HIGH ? "HIGH" : "LOW");
            
            // Set D0 HIGH
            digitalWrite(BUZZER_PIN, HIGH);
            delay(10); // Delay nhỏ để ổn định
            
            // Đọc lại trạng thái D0 sau khi set HIGH
            int stateAfter = digitalRead(BUZZER_PIN);
            Serial.print("📏 D0 sau khi set HIGH: ");
            Serial.println(stateAfter == HIGH ? "HIGH (3.3V)" : "LOW (0V)");
            
            
            
            // Test với PWM thay vì digitalWrite
            Serial.println("🔧 Thử PWM thay vì digitalWrite:");
            analogWrite(BUZZER_PIN, 1023); // PWM max
            delay(10);
            
            // Đọc trạng thái sau PWM
            int statePWM = digitalRead(BUZZER_PIN);
            Serial.print("📏 D0 sau PWM 1023: ");
            Serial.println(statePWM == HIGH ? "HIGH" : "LOW");
            
            Serial.println("   Pin D0 (GPIO16) = ACTIVE");
            Serial.println("🎯 NẾU VẪN KHÔNG KÊU: Buzzer cần transistor hoặc buzzer 3.3V");
            
            lastBuzzerState = true;
        }
    } else {
        if (lastBuzzerState) {
            // Đọc trạng thái trước khi tắt
            int stateBefore = digitalRead(BUZZER_PIN);
            Serial.print("📏 D0 trước khi set LOW: ");
            Serial.println(stateBefore == HIGH ? "HIGH" : "LOW");
            
            // Dừng kêu khi an toàn
            digitalWrite(BUZZER_PIN, LOW);
            analogWrite(BUZZER_PIN, 0);
            delay(10);
            
            // Đọc trạng thái sau khi tắt
            int stateAfter = digitalRead(BUZZER_PIN);
            Serial.print("📏 D0 sau khi set LOW: ");
            Serial.println(stateAfter == HIGH ? "HIGH" : "LOW (0V)");
            
            lastBuzzerState = false;
            Serial.println("✅ BUZZER: DỪNG KÊU - VỀ TRẠNG THÁI AN TOÀN");
            Serial.println("   Pin D0 (GPIO16) = LOW");
        }
    }
    
    // Xử lý các yêu cầu HTTP từ client - QUAN TRỌNG cho webserver
    server.handleClient();
}

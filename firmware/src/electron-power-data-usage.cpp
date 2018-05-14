#include "Particle.h"

#include "AppWatchdogWrapper.h"
#include "ConnectionCheck.h"


// SYSTEM_THREAD(ENABLED);

SYSTEM_MODE(SEMI_AUTOMATIC);

STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));


Serial1LogHandler logHandler;

const unsigned long WAIT_AFTER_CLOUD_CONNECTED = 12000;
const int TEST_SLEEP_PIN = D6;
const long SLEEP_TIME_SECS = 8;
const unsigned long DATA_REPORT_PERIOD_MS = 1000;

void runSleepTest();
void logDataUsage();

bool isCellularReady = false;
bool isCloudConnected = false;
unsigned long lastCheckData = 0;
unsigned long cloudStarted = 0;
bool buttonPressed = false;
bool runSleepTestCalled = false;
unsigned long lastDataReport = 0 - DATA_REPORT_PERIOD_MS;
int testNum = 0;
int lastTotal = 0;
unsigned long lastPublish = 0;

// Monitors the state of the connection, and sends this data using the ConnectionEvents.
// Handy for visibility.
ConnectionCheck connectionCheck;

// This is a wrapper around the ApplicationWatchdog. It just makes using it easier. It writes
// a ConnectionEvents event to retained memory then does System.reset().
AppWatchdogWrapper watchdog(60000);


enum {
	TEST_STOP_SLEEP = 0,
	TEST_STOP_SLEEP_NETWORK_STANDBY,
	TEST_SLEEP_MODE_DEEP,
	TEST_SLEEP_MODE_DEEP_NETWORK_STANDBY,
	TEST_SLEEP_MODE_DEEP_NO_SESSION,
	TEST_SLEEP_SOFT_POWER_OFF, // 5
	TEST_STOP_SLEEP_NETWORK_STANDBY_30,
	TEST_QUICK_SLEEP_DEEP_STANDBY,
	TEST_STOP_SLEEP_NETWORK_STANDBY_PUBLISH, // 8
	TEST_DEEP_SLEEP_NETWORK_STANDBY_PUBLISH,
	TEST_NO_SLEEP_PUBLISH
};

void setup() {
	Serial.begin();

	pinMode(TEST_SLEEP_PIN, INPUT_PULLDOWN);

	// Convert a binary value on D0-D3 into a testNum (D0=LSB)
	for(int ii = 0; ii < 4; ii++) {
		int pin = D0 + ii;
		pinMode(pin, INPUT_PULLUP);
		if (digitalRead(pin)) {
			testNum |= (1 << ii);
		}
	}

	Log.info("setup called testNum=%d", testNum);

	if (testNum == TEST_QUICK_SLEEP_DEEP_STANDBY) {
		System.sleep(SLEEP_MODE_DEEP, SLEEP_TIME_SECS, SLEEP_NETWORK_STANDBY);
	}

	connectionCheck.setup();

	Particle.connect();
}

void loop() {
	connectionCheck.loop();

	if (Particle.connected()) {
		if (testNum == TEST_STOP_SLEEP_NETWORK_STANDBY_PUBLISH) {
			Log.info("TEST_STOP_SLEEP_NETWORK_STANDBY_PUBLISH");
			bool bResult = Particle.publish("testSleep", "TEST_STOP_SLEEP_NETWORK_STANDBY_PUBLISH", PRIVATE, WITH_ACK);

			Log.info("publish result=%d", bResult);

			logDataUsage();

			System.sleep(TEST_SLEEP_PIN, FALLING, SLEEP_TIME_SECS, SLEEP_NETWORK_STANDBY);
		}
		else
		if (testNum == TEST_DEEP_SLEEP_NETWORK_STANDBY_PUBLISH) {
			Log.info("TEST_DEEP_SLEEP_NETWORK_STANDBY_PUBLISH");
			// 44
			bool bResult = Particle.publish("testSleep", "TEST_DEEP_SLEEP_NETWORK_STANDBY_PUBLISH", PRIVATE, WITH_ACK);

			Log.info("publish result=%d", bResult);

			logDataUsage();

			System.sleep(SLEEP_MODE_DEEP, SLEEP_TIME_SECS, SLEEP_NETWORK_STANDBY);
		}
		else
		if (testNum == TEST_NO_SLEEP_PUBLISH) {
			if (millis() - lastPublish >= 60000) {
				lastPublish = millis();

				Particle.publish("testSleep", "TEST_NO_SLEEP_PUBLISH", PRIVATE);

				logDataUsage();
			}
		}

		if (cloudStarted == 0) {
			cloudStarted = millis();
		}

		if (!runSleepTestCalled) {
			if (millis() - cloudStarted >= WAIT_AFTER_CLOUD_CONNECTED) {
				// Run this only once
				runSleepTestCalled = true;
				runSleepTest();
			}
		}
	}

	if (millis() - lastDataReport >= DATA_REPORT_PERIOD_MS) {
		lastDataReport = millis();
		logDataUsage();
	}


}

void runSleepTest() {
	switch(testNum) {
	case TEST_STOP_SLEEP:
		Log.info("TEST_STOP_SLEEP");
		delay(2000);
		System.sleep(TEST_SLEEP_PIN, FALLING, SLEEP_TIME_SECS);

		// Finished sleep, run next test
		Log.info("woke from stop sleep testNum=%d now", testNum);
		runSleepTestCalled = false;
		break;

	case TEST_STOP_SLEEP_NETWORK_STANDBY:
		Log.info("TEST_STOP_SLEEP_NETWORK_STANDBY");
		delay(2000);
		System.sleep(TEST_SLEEP_PIN, FALLING, SLEEP_TIME_SECS, SLEEP_NETWORK_STANDBY);

		// Finished sleep, run next test
		Log.info("woke from stop sleep testNum=%d now", testNum);
		runSleepTestCalled = false;
		break;

	case TEST_SLEEP_MODE_DEEP:
		Log.info("TEST_SLEEP_MODE_DEEP");
		delay(2000);
		System.sleep(SLEEP_MODE_DEEP, SLEEP_TIME_SECS);
		break;

	case TEST_SLEEP_MODE_DEEP_NETWORK_STANDBY:
		Log.info("TEST_SLEEP_MODE_DEEP_NETWORK_STANDBY");
		delay(2000);
		System.sleep(SLEEP_MODE_DEEP, SLEEP_TIME_SECS, SLEEP_NETWORK_STANDBY);
		break;

	case TEST_SLEEP_MODE_DEEP_NO_SESSION:
		Log.info("TEST_SLEEP_MODE_DEEP_NO_SESSION");
		Particle.publish("spark/device/session/end", "", PRIVATE);
		delay(2000);
		System.sleep(SLEEP_MODE_DEEP, SLEEP_TIME_SECS);
		break;

	case TEST_SLEEP_SOFT_POWER_OFF:
		Log.info("TEST_SLEEP_SOFT_POWER_OFF");
		delay(2000);
		System.sleep(SLEEP_MODE_SOFTPOWEROFF, SLEEP_TIME_SECS);
		break;

	case TEST_STOP_SLEEP_NETWORK_STANDBY_30:
		Log.info("TEST_SLEEP_MODE_DEEP_NETWORK_STANDBY_30");
		delay(2000);
		System.sleep(TEST_SLEEP_PIN, FALLING, 30 * 60, SLEEP_NETWORK_STANDBY);

		// Finished sleep, run next test
		Log.info("woke from stop sleep 30 minutes");
		runSleepTestCalled = false;
		break;

	}
}

void logDataUsage() {
	CellularData data;
	if (Cellular.getDataUsage(data)) {
		int total = data.tx_total + data.rx_total;
		if (total != lastTotal) {
			Log.info("cid=%d tx=%d rx=%d total=%d", data.cid, data.tx_total, data.rx_total, total);
			lastTotal = total;
		}
	}
}


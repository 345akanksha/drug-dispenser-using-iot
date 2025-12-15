# IoT-Based Smart Drug Dispenser for Medication Adherence

## Overview

Medication non-adherence is a significant challenge in healthcare, particularly for elderly patients and individuals with chronic illnesses. This project presents an **IoT-based smart drug dispenser** designed to ensure timely and accurate medication intake through automation, reminders, and monitoring.

The system dispenses medicines at predefined schedules, provides audio or visual alerts to users, and enables remote monitoring through an IoT framework. By integrating embedded systems with software control, the solution aims to reduce missed doses, prevent over-medication, and improve overall patient adherence.

---

## Objectives

* To design an automated drug dispensing mechanism based on scheduled timings
* To provide timely reminders using alarms or notifications
* To monitor medication intake and system status remotely
* To improve patient safety and adherence through automation

---

## System Architecture

1. **Hardware Layer**

   * Microcontroller controls the dispensing mechanism
   * Motor/servo system releases the correct dosage
   * Display and buzzer provide user alerts

2. **Scheduling and Control Logic**

   * Medication schedules are predefined and stored in the controller
   * Real-time clock (RTC) ensures accurate timing
   * Dispensing occurs only at authorized time slots

3. **IoT Communication Layer**

   * Sensor and status data is transmitted to a cloud platform
   * Enables remote monitoring of dosage events and system health

4. **User Interface**

   * Mobile or web dashboard for monitoring schedules and alerts
   * Manual override for emergency access if required

---

## Hardware Components

* Microcontroller (Arduino Uno / ESP8266 / ESP32)
* Servo motor or DC motor for pill dispensing
* Real-Time Clock (RTC) module
* Buzzer and LCD / OLED display
* Power supply and casing

---

## Software Components

* **Programming Languages:** C/C++ (Embedded), Python (optional backend)
* **Libraries and Tools:**

  * Arduino IDE
  * IoT platform libraries (MQTT / HTTP)
  * Cloud dashboard (optional)

---

## Working Principle

1. The user configures medication schedules through the system interface.
2. The real-time clock continuously tracks the current time.
3. At scheduled intervals, the controller activates the dispensing mechanism.
4. Alerts are triggered to notify the user to collect the medicine.
5. Dispensing events and system status are logged for monitoring purposes.

---

## Features

* Automated and scheduled pill dispensing
* Audio and visual reminders
* Reduced human error in dosage
* Remote monitoring through IoT integration
* Compact and user-friendly design

---

## How to Run

1. Upload the firmware code to the microcontroller using Arduino IDE.
2. Configure medication timings in the program.
3. Connect the hardware components as per the circuit diagram.
4. Power the system and verify dispensing at scheduled times.

---

## Applications

* Elderly care and assisted living
* Hospitals and clinics
* Home-based patient monitoring
* Chronic disease management

---

## Results

The system successfully dispenses medication at predefined times and provides timely reminders to users. The IoT integration enables monitoring of medication adherence, demonstrating the effectiveness of automation in healthcare support systems.

---

## Future Enhancements

* Integration with mobile applications for real-time notifications
* AI-based adherence analysis and alerts for missed doses
* Voice-based reminders for visually impaired users
* Secure cloud storage for patient medication records

---

## Contributors

Akanksha Shetty
Additional contributors (if applicable)

---

## References

1. R. Want et al., “The Internet of Things: From RFID to Smart Devices,” IEEE Computer Society.
2. A. Al-Fuqaha et al., “Internet of Things: A Survey on Enabling Technologies,” IEEE Communications Surveys.
3. Healthcare IoT Applications, IEEE Access.


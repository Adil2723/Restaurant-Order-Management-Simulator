<h1 align="center">
  🍽️ Restaurant Order Management Simulator
</h1>

<p align="center">
  A Multithreaded Operating System Simulation using C & POSIX Threads
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c" />
  <img src="https://img.shields.io/badge/OS-Project-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Threads-pthreads-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Synchronization-Semaphores%20%26%20Mutex-red?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Output-JSON-purple?style=for-the-badge" />
</p>

---

# 📌 Overview

The **Restaurant Order Management Simulator** is a real-time Operating Systems simulation project developed in **C** using:

- POSIX Threads (`pthread`)
- Mutex Locks
- Semaphores
- Condition Variables
- Priority Scheduling
- Dynamic Resource Allocation

The simulator models a real-world restaurant environment where:

✅ Customers place orders concurrently  
✅ Waiters generate requests  
✅ Chefs prepare food simultaneously  
✅ VIP customers receive higher priority  
✅ Kitchen resources are synchronized safely  
✅ Live system state is exported in JSON format  

---

# 🚀 Features

## 🍕 Core Functionalities

- 👨‍🍳 Multithreaded chef processing
- 🧾 Dynamic customer order generation
- ⭐ VIP order priority scheduling
- 🔒 Mutex-based synchronization
- 🚦 Semaphore-controlled kitchen access
- 📦 Producer-Consumer implementation
- ⚡ Adjustable simulation speed
- ❌ Live order cancellation support
- 📊 Real-time JSON state output
- 📈 Dynamic chef scaling
- 📝 Thread-safe logging system

---

# 🧠 Operating System Concepts Used

| Concept | Implementation |
|---|---|
| Producer-Consumer Problem | Waiter & Chef Threads |
| Multithreading | POSIX Threads (`pthread`) |
| Mutex Locks | Shared queue protection |
| Semaphores | Kitchen concurrency control |
| Condition Variables | Queue synchronization |
| Priority Scheduling | VIP order handling |
| Critical Section | Shared state protection |
| Dynamic Resource Allocation | Auto chef scaling |
| Synchronization | Mutex + Semaphore |

---

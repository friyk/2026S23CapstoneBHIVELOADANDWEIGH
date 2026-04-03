import tkinter as tk
import serial
import threading
import re
import math

PORT     = "/dev/ttyUSB0"
BAUDRATE = 115200

class WeightDisplay:
    def __init__(self, root):
        self.root = root
        self.root.title("B.HIVE — Live Weight")
        self.root.configure(bg="black")
        self.root.geometry("700x600")

        self.ser = None

        # -------------------------------------------------------
        # Status bar (top)
        # -------------------------------------------------------
        self.status_var = tk.StringVar(value="Connecting...")
        tk.Label(
            root,
            textvariable=self.status_var,
            font=("Helvetica", 12),
            fg="gray",
            bg="black"
        ).pack(pady=(12, 0))

        # -------------------------------------------------------
        # Weight display (large, centre)
        # -------------------------------------------------------
        self.weight_label = tk.Label(
            root,
            text="-- kg",
            font=("Helvetica", 96, "bold"),
            fg="white",
            bg="black"
        )
        self.weight_label.pack(pady=(0, 8))

        # -------------------------------------------------------
        # Serial log (scrollable text box)
        # -------------------------------------------------------
        log_frame = tk.Frame(root, bg="black")
        log_frame.pack(fill=tk.BOTH, expand=True, padx=12)

        self.log = tk.Text(
            log_frame,
            font=("Courier", 11),
            fg="#00ff99",
            bg="#111111",
            state=tk.DISABLED,
            wrap=tk.WORD,
            height=10
        )
        scrollbar = tk.Scrollbar(log_frame, command=self.log.yview)
        self.log.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # -------------------------------------------------------
        # Command input (bottom)
        # -------------------------------------------------------
        input_frame = tk.Frame(root, bg="black")
        input_frame.pack(fill=tk.X, padx=12, pady=10)

        tk.Label(
            input_frame,
            text="Command:",
            font=("Courier", 12),
            fg="gray",
            bg="black"
        ).pack(side=tk.LEFT, padx=(0, 6))

        self.cmd_entry = tk.Entry(
            input_frame,
            font=("Courier", 14),
            fg="white",
            bg="#222222",
            insertbackground="white"
        )
        self.cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.cmd_entry.bind("<Return>", self.send_command)
        self.cmd_entry.focus()

        tk.Button(
            input_frame,
            text="Send",
            font=("Courier", 12),
            fg="black",
            bg="#00ff99",
            activebackground="#00cc77",
            command=self.send_command
        ).pack(side=tk.LEFT, padx=(6, 0))

        # -------------------------------------------------------
        # Start serial thread
        # -------------------------------------------------------
        self.serial_thread = threading.Thread(target=self.read_serial, daemon=True)
        self.serial_thread.start()

    # -------------------------------------------------------
    # Append a line to the log box
    # -------------------------------------------------------
    def log_line(self, text):
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, text + "\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    # -------------------------------------------------------
    # Send command over serial
    # -------------------------------------------------------
    def send_command(self, event=None):
        cmd = self.cmd_entry.get().strip()
        if not cmd:
            return
        self.cmd_entry.delete(0, tk.END)
        if self.ser and self.ser.is_open:
            try:
                self.ser.write((cmd + "\n").encode("utf-8"))
                self.log_line(f"> {cmd}")
            except serial.SerialException as e:
                self.log_line(f"[error] Could not send: {e}")
        else:
            self.log_line("[error] Not connected.")

    # -------------------------------------------------------
    # Serial read loop (runs in background thread)
    # -------------------------------------------------------
    def read_serial(self):
        try:
            self.ser = serial.Serial(PORT, BAUDRATE, timeout=2)
            self.root.after(0, self.status_var.set, f"Connected — {PORT} @ {BAUDRATE} baud")
        except serial.SerialException as e:
            self.root.after(0, self.status_var.set, f"Error: {e}")
            return

        while True:
            try:
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                # Update weight display if this is a weight line
                match = re.search(r"Weight:\s*([-\d.]+)\s*kg", line)
                if match:
                    kg = math.floor(float(match.group(1)) * 10) / 10
                    colour = "red" if kg < 0 else "white"
                    self.root.after(0, self.weight_label.config, {"text": f"{kg:.1f} kg", "fg": colour})

                # Always log every line to the terminal box
                self.root.after(0, self.log_line, line)

            except serial.SerialException:
                self.root.after(0, self.status_var.set, "Disconnected. Reconnect ESP32 and restart.")
                self.root.after(0, self.log_line, "[disconnected]")
                break
            except Exception:
                continue

if __name__ == "__main__":
    root = tk.Tk()
    app = WeightDisplay(root)
    root.mainloop()

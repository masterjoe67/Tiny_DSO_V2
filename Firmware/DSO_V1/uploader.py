import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext
import threading
import time

class ZoeUploaderGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Zoe - NEORV32 Turbo Uploader")
        self.root.geometry("600x500")
        
        # Variabili
        self.port_var = tk.StringVar()
        self.file_path = tk.StringVar()
        self.ser = None
        self.running = False

        # --- Interfaccia ---
        # Selezione Porta
        tk.Label(root, text="Porta Seriale:").grid(row=0, column=0, padx=10, pady=10)
        self.port_menu = tk.OptionMenu(root, self.port_var, *self.get_ports())
        self.port_menu.grid(row=0, column=1, sticky="ew")
        tk.Button(root, text="Aggiorna", command=self.refresh_ports).grid(row=0, column=2, padx=5)

        # Selezione File
        tk.Label(root, text="File Binario:").grid(row=1, column=0, padx=10, pady=10)
        tk.Entry(root, textvariable=self.file_path, width=40).grid(row=1, column=1)
        tk.Button(root, text="Sfoglia", command=self.browse_file).grid(row=1, column=2, padx=5)

        # Pulsanti Azione
        self.btn_upload = tk.Button(root, text="CARICA (UPLOAD)", bg="green", fg="white", command=self.start_upload_thread)
        self.btn_upload.grid(row=2, column=1, pady=20, sticky="ew")

        # Terminale Integrato
        tk.Label(root, text="Terminale Zoe:").grid(row=3, column=0, padx=10, pady=5)
        self.terminal = scrolledtext.ScrolledText(root, height=15, width=70, bg="black", fg="lime")
        self.terminal.grid(row=4, column=0, columnspan=3, padx=10, pady=5)

    def get_ports(self):
        ports = serial.tools.list_ports.comports()
        return [p.device for p in ports] or ["Nessuna porta"]

    def refresh_ports(self):
        menu = self.port_menu["menu"]
        menu.delete(0, "end")
        for port in self.get_ports():
            menu.add_command(label=port, command=lambda p=port: self.port_var.set(p))

    def browse_file(self):
        filename = filedialog.askopenfilename(filetypes=[("Binary files", "*.bin"), ("All files", "*.*")])
        self.file_path.set(filename)

    def log(self, message):
        self.terminal.insert(tk.END, f"{message}\n")
        self.terminal.see(tk.END)

    def start_upload_thread(self):
        if not self.file_path.get() or self.port_var.get() == "Nessuna porta":
            messagebox.showerror("Errore", "Seleziona porta e file!")
            return
        threading.Thread(target=self.run_upload, daemon=True).start()

    def run_upload(self):
        try:
            self.log(f"> Apertura {self.port_var.get()}...")
            self.ser = serial.Serial(self.port_var.get(), 19200, timeout=0.1)
            self.log("> In attesa del Bootloader... (Resetta la DE0-Nano)")
            
            # Fase di attesa prompt
            bootloader_found = False
            for _ in range(100): # Timeout circa 10 secondi
                line = self.ser.readline().decode(errors='ignore').strip()
                if line:
                    self.log(f"Board: {line}")
                    if ">" in line or "CMD" in line:
                        bootloader_found = True
                        break
                time.sleep(0.1)

            if bootloader_found:
                self.log("> Bootloader pronto! Invio 'u'...")
                self.ser.write(b'u')
                time.sleep(0.5)
                
                with open(self.file_path.get(), 'rb') as f:
                    data = f.read()
                    self.log(f"> Invio {len(data)} bytes...")
                    self.ser.write(data)
                
                self.log("> Upload completato! Invio 'e' (Esegui)...")
                self.ser.write(b'e')
                
                # Entra in modalità terminale
                self.log("--- MODALITÀ TERMINALE ATTIVA ---")
                while True:
                    if self.ser.in_waiting:
                        char = self.ser.read(self.ser.in_waiting).decode(errors='ignore')
                        self.terminal.insert(tk.END, char)
                        self.terminal.see(tk.END)
            else:
                self.log("! Timeout: Bootloader non risponde.")
        
        except Exception as e:
            self.log(f"! Errore: {str(e)}")
        finally:
            if self.ser and not self.ser.is_open:
                self.ser.close()

if __name__ == "__main__":
    root = tk.Tk()
    app = ZoeUploaderGUI(root)
    root.mainloop()
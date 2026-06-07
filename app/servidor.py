from flask import Flask, jsonify, render_template, request
import threading
import time

DEVICE = "/dev/sdec_cdd"

app = Flask(__name__)

# Estado compartido
estado = {
    "valor": 0,
    "senal": 0,      # 0 = temperatura, 1 = humedad
    "error": False,
    "saltear": False,
}
lock = threading.Lock()

def leer_cdd():
    with open(DEVICE, "r") as f:
        return int(f.read().strip())

def escribir_cdd(senal):
    with open(DEVICE, "w") as f:
        f.write(str(senal))

# Hilo que lee el sensor cada 1 segundo
def tarea_sensor():
    while True:
        try:
            v = leer_cdd()
            with lock:
                if estado["saltear"]:
                    estado["saltear"] = False
                else:
                    estado["valor"] = v
                estado["error"] = False
        except Exception as e:
            with lock:
                estado["error"] = True
            print(f"Error leyendo CDD: {e}")
        time.sleep(1)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/dato")
def dato():
    with lock:
        return jsonify({
            "valor": estado["valor"],
            "senal": estado["senal"],
            "error": estado["error"],
        })

@app.route("/senal/<int:n>", methods=["POST"])
def cambiar_senal(n):
    if n not in (0, 1):
        return jsonify({"ok": False}), 400
    try:
        escribir_cdd(n)
        with lock:
            estado["senal"] = n
            estado["saltear"] = True
        return jsonify({"ok": True, "senal": n})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

if __name__ == "__main__":
    hilo = threading.Thread(target=tarea_sensor, daemon=True)
    hilo.start()
    app.run(host="0.0.0.0", port=5000, debug=False)
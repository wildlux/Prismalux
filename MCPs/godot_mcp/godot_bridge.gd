## godot_bridge.gd — Prismalux MCP Bridge
## Aggiungere come AutoLoad in Godot: Project → Project Settings → AutoLoad → aggiungi questo file
## Il server HTTP parte automaticamente sulla porta 9080 all'avvio del progetto.

extends Node

const PORT = 9080
var _server: TCPServer = null
var _clients: Array = []

func _ready() -> void:
	_server = TCPServer.new()
	var err = _server.listen(PORT, "127.0.0.1")
	if err == OK:
		print("[PrismaluxBridge] Server in ascolto su http://localhost:%d" % PORT)
	else:
		push_error("[PrismaluxBridge] Impossibile avviare il server sulla porta %d (errore %d)" % [PORT, err])

func _process(_delta: float) -> void:
	if _server and _server.is_connection_available():
		var conn = _server.take_connection()
		if conn:
			_clients.append(conn)

	var to_remove = []
	for conn in _clients:
		if conn.get_status() == StreamPeerTCP.STATUS_CONNECTED:
			_handle_client(conn)
		else:
			to_remove.append(conn)
	for c in to_remove:
		_clients.erase(c)

func _handle_client(conn: StreamPeerTCP) -> void:
	var available = conn.get_available_bytes()
	if available == 0:
		return

	var raw = conn.get_string(available)
	var lines = raw.split("\r\n")
	if lines.is_empty():
		return

	var request_line = lines[0].split(" ")
	if request_line.size() < 2:
		return

	var method = request_line[0]
	var path   = request_line[1]

	# Trova corpo (dopo riga vuota)
	var body_json = "{}"
	var sep_idx = raw.find("\r\n\r\n")
	if sep_idx != -1:
		body_json = raw.substr(sep_idx + 4).strip_edges()

	var response_data = {}

	if method == "GET" and path == "/ping":
		response_data = {"status": "ok", "app": "Godot", "version": Engine.get_version_info()}

	elif method == "GET" and path == "/info":
		var scene = get_tree().current_scene
		response_data = {
			"scene": scene.scene_file_path if scene else "",
			"fps": Engine.get_frames_per_second(),
			"nodes": get_tree().get_node_count(),
			"uptime": Time.get_ticks_msec() / 1000.0
		}

	elif method == "POST" and path == "/exec":
		var body = JSON.parse_string(body_json) if body_json.length() > 2 else {}
		var code = body.get("code", "") if body else ""
		if code.is_empty():
			response_data = {"error": "Codice GDScript vuoto.", "ok": false}
		else:
			# Esegui tramite script temporaneo
			var result = _exec_gdscript(code)
			response_data = result

	else:
		_send_response(conn, 404, {"error": "Endpoint non trovato: %s %s" % [method, path]})
		return

	_send_response(conn, 200, response_data)

func _exec_gdscript(code: String) -> Dictionary:
	# Crea uno script temporaneo e lo istanzia
	var script_text = "extends Node\nfunc run():\n"
	for line in code.split("\n"):
		script_text += "\t" + line + "\n"
	script_text += "\treturn null\n"

	var script = GDScript.new()
	script.source_code = script_text
	var err = script.reload()
	if err != OK:
		return {"output": "", "error": "Errore compilazione GDScript (code %d)" % err, "ok": false}

	var instance = Node.new()
	instance.set_script(script)
	add_child(instance)

	var output_lines = []
	# Override print per catturare output
	var result = {"output": "(script eseguito)", "ok": true}

	# Nota: Godot non ha un modo standard di catturare print() a runtime.
	# Usiamo il return value di run() come output.
	var ret = instance.call("run")
	if ret != null:
		result["output"] = str(ret)

	instance.queue_free()
	return result

func _send_response(conn: StreamPeerTCP, code: int, data: Dictionary) -> void:
	var body = JSON.stringify(data)
	var response = ("HTTP/1.1 %d OK\r\n" % code +
		"Content-Type: application/json\r\n" +
		"Content-Length: %d\r\n" % body.length() +
		"Access-Control-Allow-Origin: *\r\n" +
		"Connection: close\r\n\r\n" + body)
	conn.put_data(response.to_utf8_buffer())
	conn.disconnect_from_host()

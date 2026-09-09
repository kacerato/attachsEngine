extends SceneTree

func _initialize() -> void:
    call_deferred("validar")

func validar() -> void:
    var scene := Node3D.new()
    scene.name = "Cena"
    root.add_child(scene)
    var cube := MeshInstance3D.new()
    cube.name = "Objeto"
    cube.mesh = BoxMesh.new()
    scene.add_child(cube)
    cube.owner = scene
    cube.set_meta("identidade", "validacao-objeto-1")
    var commands := UndoRedo.new()
    commands.create_action("Mover objeto")
    commands.add_do_property(cube, "position", Vector3(2, 1, 0))
    commands.add_undo_property(cube, "position", Vector3.ZERO)
    commands.commit_action()
    assert(cube.position == Vector3(2, 1, 0), "Transformação")
    commands.undo()
    assert(cube.position == Vector3.ZERO, "Desfazer")
    commands.redo()
    assert(cube.position == Vector3(2, 1, 0), "Refazer")
    var camera := Camera3D.new()
    camera.name = "Camera"
    scene.add_child(camera)
    camera.owner = scene
    camera.position = Vector3(0, 2, 6)
    camera.look_at(Vector3.ZERO)
    camera.current = true
    assert(root.get_camera_3d() == camera, "Câmera do viewport")
    var packed := PackedScene.new()
    assert(packed.pack(scene) == OK, "Empacotar cena")
    assert(ResourceSaver.save(packed, "user://validacao.tscn") == OK, "Salvar cena")
    var loaded := load("user://validacao.tscn") as PackedScene
    var restored := loaded.instantiate()
    assert(restored.get_node("Objeto").position == Vector3(2, 1, 0), "Restaurar transformação")
    assert(restored.get_node("Objeto").get_meta("identidade") == "validacao-objeto-1", "Restaurar identidade")
    assert(restored.get_node("Camera") is Camera3D, "Restaurar câmera")
    restored.free()
    commands.clear_history()
    commands.free()
    scene.free()
    print("ASTRA_GODOT_VALIDACAO_OK: objetos, viewport, transformação, desfazer/refazer, salvar/abrir")
    quit(0)

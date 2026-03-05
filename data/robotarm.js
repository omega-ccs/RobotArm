var servo_pos_rotation = 90;
var servo_pos_shoulder = 90;
var servo_pos_claw = 90;

// Canvas element and its 2d context
var canvas;
var context;

var ws;

function robotarm() {
    canvas = document.getElementById("canvas")
    context = canvas.getContext("2d")

    // Register the keydown() function with the browser's "keydown" event
    document.addEventListener("keydown", keydown);
    // Register the keyup() function with the browser's "keyup" event
    document.addEventListener("keyup", keyup);

    // Whenever a gamepad is "attached" (including at startup), grab them and output some details
    window.addEventListener("gamepadconnected", (event) => {
        console.log("Gamepad connected at index %d: %s. %d buttons, %d axes.",
                    event.gamepad.index, event.gamepad.id,
            event.gamepad.buttons.length, event.gamepad.axes.length);
        console.log(event.gamepad.id)
    }, false);

    ws = new WebSocket(`ws://${robotarm_hostname}/ws`)

    // Run the main loop
    draw();
}

var robotarm_hostname = "robotarm.local"
//var robotarm_hostname = "10.0.128.97"

function draw() {
    // Clear the canvas
    context.clearRect(0,0,canvas.width,canvas.height)

    // Assuming there is a gamepad attached
    gamepads = navigator.getGamepads();
    if (gamepads[0]) {
        // Grab the X and Y axes and adjust them to the servo range
        servo_pos_rotation = (gamepads[0].axes[0] * -90) + 90; 
        servo_pos_shoulder = (gamepads[0].axes[1] * -90) + 90;

        var clawbutton;
        //console.log(gamepads[0].buttons.map(button => button.pressed))
        if (gamepads[0].id == "Saitek ST90 USB Stick (Vendor: 06a3 Product: 0422)")
            clawbutton = gamepads[0].buttons[0]
        else
            clawbutton = gamepads[0].buttons[42]

        // Set claw servo to 67 if pressed, otherwise to 42
        servo_pos_claw = clawbutton.pressed ? 67 : 42
    }

    // Draw the servo positions to the screen
    context.fillStyle = "blue";
    context.font = "40px sans-serif"
    context.fillText(servo_pos_rotation,100,100);
    context.fillText(servo_pos_shoulder,100,200);

    var ws_message = {
        "rotation": servo_pos_rotation,
        "shoulder": servo_pos_shoulder,
        "claw": servo_pos_claw
    }
    
    // only if the websocket is open and ready, send messages
    if (ws.readyState == WebSocket.OPEN) {
        ws_message_json = JSON.stringify(ws_message)
        console.log(`sending ws message ${ws_message_json}`)
        ws.send(ws_message_json)
    }
    
    // use fetch/HTTP to send servo positions
    fetch(`http://${robotarm_hostname}/servo?rotation=${servo_pos_rotation}&shoulder=${servo_pos_shoulder}`)

    // Either request the browser call this function ASAP
//    window.requestAnimationFrame(draw);
    // or just use a timer to ensure it doesn't run too frequently
    setTimeout(draw,100);
}

function keydown(event) {
    if (event.key == "ArrowLeft") {

    } else if (event.key == "ArrowRight") {

    }
}

function keyup(event) {
    if (event.key == "ArrowLeft" || event.key == "ArrowRight") {

    }
}


   function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
  }


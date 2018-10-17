//"use strict";

// Utilities for browser environments
var LibraryBrowser = {
  $Browser__deps: ['emscripten_set_main_loop', 'emscripten_set_main_loop_timing'],
  $Browser__postset: 'Module["requestFullScreen"] = function Module_requestFullScreen(lockPointer, resizeCanvas, vrDevice) { err("Module.requestFullScreen is deprecated. Please call Module.requestFullscreen instead."); Module["requestFullScreen"] = Module["requestFullscreen"]; Browser.requestFullScreen(lockPointer, resizeCanvas, vrDevice) };\n' + // exports
                     'Module["requestFullscreen"] = function Module_requestFullscreen(lockPointer, resizeCanvas, vrDevice) { Browser.requestFullscreen(lockPointer, resizeCanvas, vrDevice) };\n' + // exports
                     'Module["requestAnimationFrame"] = function Module_requestAnimationFrame(func) { Browser.requestAnimationFrame(func) };\n' +
                     'Module["setCanvasSize"] = function Module_setCanvasSize(width, height, noUpdates) { Browser.setCanvasSize(width, height, noUpdates) };\n' +
                     'Module["pauseMainLoop"] = function Module_pauseMainLoop() { Browser.mainLoop.pause() };\n' +
                     'Module["resumeMainLoop"] = function Module_resumeMainLoop() { Browser.mainLoop.resume() };\n' +
                     'Module["getUserMedia"] = function Module_getUserMedia() { Browser.getUserMedia() }\n' +
                     'Module["createContext"] = function Module_createContext(canvas, useWebGL, setInModule, webGLContextAttributes) { return Browser.createContext(canvas, useWebGL, setInModule, webGLContextAttributes) }',
  $Browser: {
    mainLoop: {
      scheduler: null,
      method: '',
      // Each main loop is numbered with a ID in sequence order. Only one main loop can run at a time. This variable stores the ordinal number of the main loop that is currently
      // allowed to run. All previous main loops will quit themselves. This is incremented whenever a new main loop is created.
      currentlyRunningMainloop: 0,
      func: null, // The main loop tick function that will be called at each iteration.
      arg: 0, // The argument that will be passed to the main loop. (of type void*)
      timingMode: 0,
      timingValue: 0,
      currentFrameNumber: 0,
      queue: [],
      pause: function() {
        Browser.mainLoop.scheduler = null;
        Browser.mainLoop.currentlyRunningMainloop++; // Incrementing this signals the previous main loop that it's now become old, and it must return.
      },
      resume: function() {
        Browser.mainLoop.currentlyRunningMainloop++;
        var timingMode = Browser.mainLoop.timingMode;
        var timingValue = Browser.mainLoop.timingValue;
        var func = Browser.mainLoop.func;
        Browser.mainLoop.func = null;
        _emscripten_set_main_loop(func, 0, false, Browser.mainLoop.arg, true /* do not set timing and call scheduler, we will do it on the next lines */);
        _emscripten_set_main_loop_timing(timingMode, timingValue);
        Browser.mainLoop.scheduler();
      },
      updateStatus: function() {
        if (Module['setStatus']) {
          var message = Module['statusMessage'] || 'Please wait...';
          var remaining = Browser.mainLoop.remainingBlockers;
          var expected = Browser.mainLoop.expectedBlockers;
          if (remaining) {
            if (remaining < expected) {
              Module['setStatus'](message + ' (' + (expected - remaining) + '/' + expected + ')');
            } else {
              Module['setStatus'](message);
            }
          } else {
            Module['setStatus']('');
          }
        }
      },
      runIter: function(func) {
        if (ABORT) return;
        if (Module['preMainLoop']) {
          var preRet = Module['preMainLoop']();
          if (preRet === false) {
            return; // |return false| skips a frame
          }
        }
        try {
          func();
        } catch (e) {
          if (e instanceof ExitStatus) {
            return;
          } else {
            if (e && typeof e === 'object' && e.stack) err('exception thrown: ' + [e, e.stack]);
            throw e;
          }
        }
        if (Module['postMainLoop']) Module['postMainLoop']();
      }
    },
    isFullscreen: false,
    pointerLock: false,
    moduleContextCreatedCallbacks: [],
    workers: [],

    init: function() {
      if (!Module["preloadPlugins"]) Module["preloadPlugins"] = []; // needs to exist even in workers

      if (Browser.initted) return;
      Browser.initted = true;

      try {
        new Blob();
        Browser.hasBlobConstructor = true;
      } catch(e) {
        Browser.hasBlobConstructor = false;
        console.log("warning: no blob constructor, cannot create blobs with mimetypes");
      }
      Browser.BlobBuilder = typeof MozBlobBuilder != "undefined" ? MozBlobBuilder : (typeof WebKitBlobBuilder != "undefined" ? WebKitBlobBuilder : (!Browser.hasBlobConstructor ? console.log("warning: no BlobBuilder") : null));
      Browser.URLObject = typeof window != "undefined" ? (window.URL ? window.URL : window.webkitURL) : undefined;
      if (!Module.noImageDecoding && typeof Browser.URLObject === 'undefined') {
        console.log("warning: Browser does not support creating object URLs. Built-in browser image decoding will not be available.");
        Module.noImageDecoding = true;
      }

      // Support for plugins that can process preloaded files. You can add more of these to
      // your app by creating and appending to Module.preloadPlugins.
      //
      // Each plugin is asked if it can handle a file based on the file's name. If it can,
      // it is given the file's raw data. When it is done, it calls a callback with the file's
      // (possibly modified) data. For example, a plugin might decompress a file, or it
      // might create some side data structure for use later (like an Image element, etc.).

      var imagePlugin = {};
      imagePlugin['canHandle'] = function imagePlugin_canHandle(name) {
        return !Module.noImageDecoding && /\.(jpg|jpeg|png|bmp)$/i.test(name);
      };
      imagePlugin['handle'] = function imagePlugin_handle(byteArray, name, onload, onerror) {
        var b = null;
        if (Browser.hasBlobConstructor) {
          try {
            b = new Blob([byteArray], { type: Browser.getMimetype(name) });
            if (b.size !== byteArray.length) { // Safari bug #118630
              // Safari's Blob can only take an ArrayBuffer
              b = new Blob([(new Uint8Array(byteArray)).buffer], { type: Browser.getMimetype(name) });
            }
          } catch(e) {
            warnOnce('Blob constructor present but fails: ' + e + '; falling back to blob builder');
          }
        }
        if (!b) {
          var bb = new Browser.BlobBuilder();
          bb.append((new Uint8Array(byteArray)).buffer); // we need to pass a buffer, and must copy the array to get the right data range
          b = bb.getBlob();
        }
        var url = Browser.URLObject.createObjectURL(b);
#if ASSERTIONS
        assert(typeof url == 'string', 'createObjectURL must return a url as a string');
#endif
        var img = new Image();
        img.onload = function img_onload() {
          assert(img.complete, 'Image ' + name + ' could not be decoded');
          var canvas = document.createElement('canvas');
          canvas.width = img.width;
          canvas.height = img.height;
          var ctx = canvas.getContext('2d');
          ctx.drawImage(img, 0, 0);
          Module["preloadedImages"][name] = canvas;
          Browser.URLObject.revokeObjectURL(url);
          if (onload) onload(byteArray);
        };
        img.onerror = function img_onerror(event) {
          console.log('Image ' + url + ' could not be decoded');
          if (onerror) onerror();
        };
        img.src = url;
      };
      Module['preloadPlugins'].push(imagePlugin);

      var audioPlugin = {};
      audioPlugin['canHandle'] = function audioPlugin_canHandle(name) {
        return !Module.noAudioDecoding && name.substr(-4) in { '.ogg': 1, '.wav': 1, '.mp3': 1 };
      };
      audioPlugin['handle'] = function audioPlugin_handle(byteArray, name, onload, onerror) {
        var done = false;
        function finish(audio) {
          if (done) return;
          done = true;
          Module["preloadedAudios"][name] = audio;
          if (onload) onload(byteArray);
        }
        function fail() {
          if (done) return;
          done = true;
          Module["preloadedAudios"][name] = new Audio(); // empty shim
          if (onerror) onerror();
        }
        if (Browser.hasBlobConstructor) {
          try {
            var b = new Blob([byteArray], { type: Browser.getMimetype(name) });
          } catch(e) {
            return fail();
          }
          var url = Browser.URLObject.createObjectURL(b); // XXX we never revoke this!
#if ASSERTIONS
          assert(typeof url == 'string', 'createObjectURL must return a url as a string');
#endif
          var audio = new Audio();
          audio.addEventListener('canplaythrough', function() { finish(audio) }, false); // use addEventListener due to chromium bug 124926
          audio.onerror = function audio_onerror(event) {
            if (done) return;
            console.log('warning: browser could not fully decode audio ' + name + ', trying slower base64 approach');
            function encode64(data) {
              var BASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
              var PAD = '=';
              var ret = '';
              var leftchar = 0;
              var leftbits = 0;
              for (var i = 0; i < data.length; i++) {
                leftchar = (leftchar << 8) | data[i];
                leftbits += 8;
                while (leftbits >= 6) {
                  var curr = (leftchar >> (leftbits-6)) & 0x3f;
                  leftbits -= 6;
                  ret += BASE[curr];
                }
              }
              if (leftbits == 2) {
                ret += BASE[(leftchar&3) << 4];
                ret += PAD + PAD;
              } else if (leftbits == 4) {
                ret += BASE[(leftchar&0xf) << 2];
                ret += PAD;
              }
              return ret;
            }
            audio.src = 'data:audio/x-' + name.substr(-3) + ';base64,' + encode64(byteArray);
            finish(audio); // we don't wait for confirmation this worked - but it's worth trying
          };
          audio.src = url;
          // workaround for chrome bug 124926 - we do not always get oncanplaythrough or onerror
          Browser.safeSetTimeout(function() {
            finish(audio); // try to use it even though it is not necessarily ready to play
          }, 10000);
        } else {
          return fail();
        }
      };
      Module['preloadPlugins'].push(audioPlugin);

#if WASM
#if MAIN_MODULE
      var wasmPlugin = {};
      wasmPlugin['asyncWasmLoadPromise'] = new Promise(
        function(resolve, reject) { return resolve(); });
      wasmPlugin['canHandle'] = function(name) {
        return !Module.noWasmDecoding && name.endsWith('.so');
      };
      wasmPlugin['handle'] = function(byteArray, name, onload, onerror) {
        // loadWebAssemblyModule can not load modules out-of-order, so rather
        // than just running the promises in parallel, this makes a chain of
        // promises to run in series.
        this['asyncWasmLoadPromise'] = this['asyncWasmLoadPromise'].then(
          function() {
            return loadWebAssemblyModule(byteArray, true);
          }).then(
            function(module) {
              Module['preloadedWasm'][name] = module;
              onload();
            },
            function(err) {
              console.warn("Couldn't instantiate wasm: " + name + " '" + err + "'");
              onerror();
            });
      };
      Module['preloadPlugins'].push(wasmPlugin);
#endif // MAIN_MODULE
#endif // WASM

      // Canvas event setup

      function pointerLockChange() {
        Browser.pointerLock = document['pointerLockElement'] === Module['canvas'] ||
                              document['mozPointerLockElement'] === Module['canvas'] ||
                              document['webkitPointerLockElement'] === Module['canvas'] ||
                              document['msPointerLockElement'] === Module['canvas'];
      }
      var canvas = Module['canvas'];
      if (canvas) {
        // forced aspect ratio can be enabled by defining 'forcedAspectRatio' on Module
        // Module['forcedAspectRatio'] = 4 / 3;

        canvas.requestPointerLock = canvas['requestPointerLock'] ||
                                    canvas['mozRequestPointerLock'] ||
                                    canvas['webkitRequestPointerLock'] ||
                                    canvas['msRequestPointerLock'] ||
                                    function(){};
        canvas.exitPointerLock = document['exitPointerLock'] ||
                                 document['mozExitPointerLock'] ||
                                 document['webkitExitPointerLock'] ||
                                 document['msExitPointerLock'] ||
                                 function(){}; // no-op if function does not exist
        canvas.exitPointerLock = canvas.exitPointerLock.bind(document);

        document.addEventListener('pointerlockchange', pointerLockChange, false);
        document.addEventListener('mozpointerlockchange', pointerLockChange, false);
        document.addEventListener('webkitpointerlockchange', pointerLockChange, false);
        document.addEventListener('mspointerlockchange', pointerLockChange, false);

        if (Module['elementPointerLock']) {
          canvas.addEventListener("click", function(ev) {
            if (!Browser.pointerLock && Module['canvas'].requestPointerLock) {
              Module['canvas'].requestPointerLock();
              ev.preventDefault();
            }
          }, false);
        }
      }
    },

    createContext: function(canvas, useWebGL, setInModule, webGLContextAttributes) {
      if (useWebGL && Module.ctx && canvas == Module.canvas) return Module.ctx; // no need to recreate GL context if it's already been created for this canvas.

      var ctx;
      var contextHandle;
      if (useWebGL) {
        // For GLES2/desktop GL compatibility, adjust a few defaults to be different to WebGL defaults, so that they align better with the desktop defaults.
        var contextAttributes = {
          antialias: false,
          alpha: false
        };

        if (webGLContextAttributes) {
          for (var attribute in webGLContextAttributes) {
            contextAttributes[attribute] = webGLContextAttributes[attribute];
          }
        }

        contextHandle = GL.createContext(canvas, contextAttributes);
        if (contextHandle) {
          ctx = GL.getContext(contextHandle).GLctx;
        }
      } else {
        ctx = canvas.getContext('2d');
      }

      if (!ctx) return null;

      if (setInModule) {
        if (!useWebGL) assert(typeof GLctx === 'undefined', 'cannot set in module if GLctx is used, but we are a non-GL context that would replace it');

        Module.ctx = ctx;
        if (useWebGL) GL.makeContextCurrent(contextHandle);
        Module.useWebGL = useWebGL;
        Browser.moduleContextCreatedCallbacks.forEach(function(callback) { callback() });
        Browser.init();
      }
      return ctx;
    },

    destroyContext: function(canvas, useWebGL, setInModule) {},

    fullscreenHandlersInstalled: false,
    lockPointer: undefined,
    resizeCanvas: undefined,
    requestFullscreen: function(lockPointer, resizeCanvas, vrDevice) {
      Browser.lockPointer = lockPointer;
      Browser.resizeCanvas = resizeCanvas;
      Browser.vrDevice = vrDevice;
      if (typeof Browser.lockPointer === 'undefined') Browser.lockPointer = true;
      if (typeof Browser.resizeCanvas === 'undefined') Browser.resizeCanvas = false;
      if (typeof Browser.vrDevice === 'undefined') Browser.vrDevice = null;

      var canvas = Module['canvas'];
      function fullscreenChange() {
        Browser.isFullscreen = false;
        var canvasContainer = canvas.parentNode;
        if ((document['fullscreenElement'] || document['mozFullScreenElement'] ||
             document['msFullscreenElement'] || document['webkitFullscreenElement'] ||
             document['webkitCurrentFullScreenElement']) === canvasContainer) {
          canvas.exitFullscreen = document['exitFullscreen'] ||
                                  document['cancelFullScreen'] ||
                                  document['mozCancelFullScreen'] ||
                                  document['msExitFullscreen'] ||
                                  document['webkitCancelFullScreen'] ||
                                  function() {};
          canvas.exitFullscreen = canvas.exitFullscreen.bind(document);
          if (Browser.lockPointer) canvas.requestPointerLock();
          Browser.isFullscreen = true;
          if (Browser.resizeCanvas) {
            Browser.setFullscreenCanvasSize();
          } else {
            Browser.updateCanvasDimensions(canvas);
          }
        } else {
          // remove the full screen specific parent of the canvas again to restore the HTML structure from before going full screen
          canvasContainer.parentNode.insertBefore(canvas, canvasContainer);
          canvasContainer.parentNode.removeChild(canvasContainer);

          if (Browser.resizeCanvas) {
            Browser.setWindowedCanvasSize();
          } else {
            Browser.updateCanvasDimensions(canvas);
          }
        }
        if (Module['onFullScreen']) Module['onFullScreen'](Browser.isFullscreen);
        if (Module['onFullscreen']) Module['onFullscreen'](Browser.isFullscreen);
      }

      if (!Browser.fullscreenHandlersInstalled) {
        Browser.fullscreenHandlersInstalled = true;
        document.addEventListener('fullscreenchange', fullscreenChange, false);
        document.addEventListener('mozfullscreenchange', fullscreenChange, false);
        document.addEventListener('webkitfullscreenchange', fullscreenChange, false);
        document.addEventListener('MSFullscreenChange', fullscreenChange, false);
      }

      // create a new parent to ensure the canvas has no siblings. this allows browsers to optimize full screen performance when its parent is the full screen root
      var canvasContainer = document.createElement("div");
      canvas.parentNode.insertBefore(canvasContainer, canvas);
      canvasContainer.appendChild(canvas);

      // use parent of canvas as full screen root to allow aspect ratio correction (Firefox stretches the root to screen size)
      canvasContainer.requestFullscreen = canvasContainer['requestFullscreen'] ||
                                          canvasContainer['mozRequestFullScreen'] ||
                                          canvasContainer['msRequestFullscreen'] ||
                                         (canvasContainer['webkitRequestFullscreen'] ? function() { canvasContainer['webkitRequestFullscreen'](Element['ALLOW_KEYBOARD_INPUT']) } : null) ||
                                         (canvasContainer['webkitRequestFullScreen'] ? function() { canvasContainer['webkitRequestFullScreen'](Element['ALLOW_KEYBOARD_INPUT']) } : null);

      if (vrDevice) {
        canvasContainer.requestFullscreen({ vrDisplay: vrDevice });
      } else {
        canvasContainer.requestFullscreen();
      }
    },

    requestFullScreen: function(lockPointer, resizeCanvas, vrDevice) {
        err('Browser.requestFullScreen() is deprecated. Please call Browser.requestFullscreen instead.');
        Browser.requestFullScreen = function(lockPointer, resizeCanvas, vrDevice) {
          return Browser.requestFullscreen(lockPointer, resizeCanvas, vrDevice);
        }
        return Browser.requestFullscreen(lockPointer, resizeCanvas, vrDevice);
    },

    nextRAF: 0,

    fakeRequestAnimationFrame: function(func) {
      // try to keep 60fps between calls to here
      var now = Date.now();
      if (Browser.nextRAF === 0) {
        Browser.nextRAF = now + 1000/60;
      } else {
        while (now + 2 >= Browser.nextRAF) { // fudge a little, to avoid timer jitter causing us to do lots of delay:0
          Browser.nextRAF += 1000/60;
        }
      }
      var delay = Math.max(Browser.nextRAF - now, 0);
      setTimeout(func, delay);
    },

    requestAnimationFrame: function requestAnimationFrame(func) {
      if (typeof window === 'undefined') { // Provide fallback to setTimeout if window is undefined (e.g. in Node.js)
        Browser.fakeRequestAnimationFrame(func);
      } else {
        if (!window.requestAnimationFrame) {
          window.requestAnimationFrame = window['requestAnimationFrame'] ||
                                         window['mozRequestAnimationFrame'] ||
                                         window['webkitRequestAnimationFrame'] ||
                                         window['msRequestAnimationFrame'] ||
                                         window['oRequestAnimationFrame'] ||
                                         Browser.fakeRequestAnimationFrame;
        }
        window.requestAnimationFrame(func);
      }
    },

    // generic abort-aware wrapper for an async callback
    safeCallback: function(func) {
      return function() {
        if (!ABORT) return func.apply(null, arguments);
      };
    },

    // abort and pause-aware versions TODO: build main loop on top of this?

    allowAsyncCallbacks: true,
    queuedAsyncCallbacks: [],

    pauseAsyncCallbacks: function() {
      Browser.allowAsyncCallbacks = false;
    },
    resumeAsyncCallbacks: function() { // marks future callbacks as ok to execute, and synchronously runs any remaining ones right now
      Browser.allowAsyncCallbacks = true;
      if (Browser.queuedAsyncCallbacks.length > 0) {
        var callbacks = Browser.queuedAsyncCallbacks;
        Browser.queuedAsyncCallbacks = [];
        callbacks.forEach(function(func) {
          func();
        });
      }
    },

    safeRequestAnimationFrame: function(func) {
      return Browser.requestAnimationFrame(function() {
        if (ABORT) return;
        if (Browser.allowAsyncCallbacks) {
          func();
        } else {
          Browser.queuedAsyncCallbacks.push(func);
        }
      });
    },
    safeSetTimeout: function(func, timeout) {
      Module['noExitRuntime'] = true;
      return setTimeout(function() {
        if (ABORT) return;
        if (Browser.allowAsyncCallbacks) {
          func();
        } else {
          Browser.queuedAsyncCallbacks.push(func);
        }
      }, timeout);
    },
    safeSetInterval: function(func, timeout) {
      Module['noExitRuntime'] = true;
      return setInterval(function() {
        if (ABORT) return;
        if (Browser.allowAsyncCallbacks) {
          func();
        } // drop it on the floor otherwise, next interval will kick in
      }, timeout);
    },

    getMimetype: function(name) {
      return {
        'jpg': 'image/jpeg',
        'jpeg': 'image/jpeg',
        'png': 'image/png',
        'bmp': 'image/bmp',
        'ogg': 'audio/ogg',
        'wav': 'audio/wav',
        'mp3': 'audio/mpeg'
      }[name.substr(name.lastIndexOf('.')+1)];
    },

    getUserMedia: function(func) {
      if(!window.getUserMedia) {
        window.getUserMedia = navigator['getUserMedia'] ||
                              navigator['mozGetUserMedia'];
      }
      window.getUserMedia(func);
    },


    getMovementX: function(event) {
      return event['movementX'] ||
             event['mozMovementX'] ||
             event['webkitMovementX'] ||
             0;
    },

    getMovementY: function(event) {
      return event['movementY'] ||
             event['mozMovementY'] ||
             event['webkitMovementY'] ||
             0;
    },

    // Browsers specify wheel direction according to the page CSS pixel Y direction:
    // Scrolling mouse wheel down (==towards user/away from screen) on Windows/Linux (and OSX without 'natural scroll' enabled)
    // is the positive wheel direction. Scrolling mouse wheel up (towards the screen) is the negative wheel direction.
    // This function returns the wheel direction in the browser page coordinate system (+: down, -: up). Note that this is often the
    // opposite of native code: In native APIs the positive scroll direction is to scroll up (away from the user).
    // NOTE: The mouse wheel delta is a decimal number, and can be a fractional value within -1 and 1. If you need to represent
    //       this as an integer, don't simply cast to int, or you may receive scroll events for wheel delta == 0.
    getMouseWheelDelta: function(event) {
      var delta = 0;
      switch (event.type) {
        case 'DOMMouseScroll':
          delta = event.detail;
          break;
        case 'mousewheel':
          delta = event.wheelDelta;
          break;
        case 'wheel':
          delta = event['deltaY'];
          break;
        default:
          throw 'unrecognized mouse wheel event: ' + event.type;
      }
      return delta;
    },

    mouseX: 0,
    mouseY: 0,
    mouseMovementX: 0,
    mouseMovementY: 0,
    touches: {},
    lastTouches: {},

    calculateMouseEvent: function(event) { // event should be mousemove, mousedown or mouseup
      if (Browser.pointerLock) {
        // When the pointer is locked, calculate the coordinates
        // based on the movement of the mouse.
        // Workaround for Firefox bug 764498
        if (event.type != 'mousemove' &&
            ('mozMovementX' in event)) {
          Browser.mouseMovementX = Browser.mouseMovementY = 0;
        } else {
          Browser.mouseMovementX = Browser.getMovementX(event);
          Browser.mouseMovementY = Browser.getMovementY(event);
        }

        // check if SDL is available
        if (typeof SDL != "undefined") {
          Browser.mouseX = SDL.mouseX + Browser.mouseMovementX;
          Browser.mouseY = SDL.mouseY + Browser.mouseMovementY;
        } else {
          // just add the mouse delta to the current absolut mouse position
          // FIXME: ideally this should be clamped against the canvas size and zero
          Browser.mouseX += Browser.mouseMovementX;
          Browser.mouseY += Browser.mouseMovementY;
        }
      } else {
        // Otherwise, calculate the movement based on the changes
        // in the coordinates.
        var rect = Module["canvas"].getBoundingClientRect();
        var cw = Module["canvas"].width;
        var ch = Module["canvas"].height;

        // Neither .scrollX or .pageXOffset are defined in a spec, but
        // we prefer .scrollX because it is currently in a spec draft.
        // (see: http://www.w3.org/TR/2013/WD-cssom-view-20131217/)
        var scrollX = ((typeof window.scrollX !== 'undefined') ? window.scrollX : window.pageXOffset);
        var scrollY = ((typeof window.scrollY !== 'undefined') ? window.scrollY : window.pageYOffset);
#if ASSERTIONS
        // If this assert lands, it's likely because the browser doesn't support scrollX or pageXOffset
        // and we have no viable fallback.
        assert((typeof scrollX !== 'undefined') && (typeof scrollY !== 'undefined'), 'Unable to retrieve scroll position, mouse positions likely broken.');
#endif

        if (event.type === 'touchstart' || event.type === 'touchend' || event.type === 'touchmove') {
          var touch = event.touch;
          if (touch === undefined) {
            return; // the "touch" property is only defined in SDL

          }
          var adjustedX = touch.pageX - (scrollX + rect.left);
          var adjustedY = touch.pageY - (scrollY + rect.top);

          adjustedX = adjustedX * (cw / rect.width);
          adjustedY = adjustedY * (ch / rect.height);

          var coords = { x: adjustedX, y: adjustedY };

          if (event.type === 'touchstart') {
            Browser.lastTouches[touch.identifier] = coords;
            Browser.touches[touch.identifier] = coords;
          } else if (event.type === 'touchend' || event.type === 'touchmove') {
            var last = Browser.touches[touch.identifier];
            if (!last) last = coords;
            Browser.lastTouches[touch.identifier] = last;
            Browser.touches[touch.identifier] = coords;
          }
          return;
        }

        var x = event.pageX - (scrollX + rect.left);
        var y = event.pageY - (scrollY + rect.top);

        // the canvas might be CSS-scaled compared to its backbuffer;
        // SDL-using content will want mouse coordinates in terms
        // of backbuffer units.
        x = x * (cw / rect.width);
        y = y * (ch / rect.height);

        Browser.mouseMovementX = x - Browser.mouseX;
        Browser.mouseMovementY = y - Browser.mouseY;
        Browser.mouseX = x;
        Browser.mouseY = y;
      }
    },

    asyncLoad: function(url, onload, onerror, noRunDep) {
      var dep = !noRunDep ? getUniqueRunDependency('al ' + url) : '';
      Module['readAsync'](url, function(arrayBuffer) {
        assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
        onload(new Uint8Array(arrayBuffer));
        if (dep) removeRunDependency(dep);
      }, function(event) {
        if (onerror) {
          onerror();
        } else {
          throw 'Loading data file "' + url + '" failed.';
        }
      });
      if (dep) addRunDependency(dep);
    },

    resizeListeners: [],

    updateResizeListeners: function() {
      var canvas = Module['canvas'];
      Browser.resizeListeners.forEach(function(listener) {
        listener(canvas.width, canvas.height);
      });
    },

    setCanvasSize: function(width, height, noUpdates) {
      var canvas = Module['canvas'];
      Browser.updateCanvasDimensions(canvas, width, height);
      if (!noUpdates) Browser.updateResizeListeners();
    },

    windowedWidth: 0,
    windowedHeight: 0,
    setFullscreenCanvasSize: function() {
      // check if SDL is available
      if (typeof SDL != "undefined") {
        var flags = {{{ makeGetValue('SDL.screen', '0', 'i32', 0, 1) }}};
        flags = flags | 0x00800000; // set SDL_FULLSCREEN flag
        {{{ makeSetValue('SDL.screen', '0', 'flags', 'i32') }}}
      }
      Browser.updateCanvasDimensions(Module['canvas']);
      Browser.updateResizeListeners();
    },

    setWindowedCanvasSize: function() {
      // check if SDL is available
      if (typeof SDL != "undefined") {
        var flags = {{{ makeGetValue('SDL.screen', '0', 'i32', 0, 1) }}};
        flags = flags & ~0x00800000; // clear SDL_FULLSCREEN flag
        {{{ makeSetValue('SDL.screen', '0', 'flags', 'i32') }}}
      }
      Browser.updateCanvasDimensions(Module['canvas']);
      Browser.updateResizeListeners();
    },

    updateCanvasDimensions : function(canvas, wNative, hNative) {
      if (wNative && hNative) {
        canvas.widthNative = wNative;
        canvas.heightNative = hNative;
      } else {
        wNative = canvas.widthNative;
        hNative = canvas.heightNative;
      }
      var w = wNative;
      var h = hNative;
      if (Module['forcedAspectRatio'] && Module['forcedAspectRatio'] > 0) {
        if (w/h < Module['forcedAspectRatio']) {
          w = Math.round(h * Module['forcedAspectRatio']);
        } else {
          h = Math.round(w / Module['forcedAspectRatio']);
        }
      }
      if (((document['fullscreenElement'] || document['mozFullScreenElement'] ||
           document['msFullscreenElement'] || document['webkitFullscreenElement'] ||
           document['webkitCurrentFullScreenElement']) === canvas.parentNode) && (typeof screen != 'undefined')) {
         var factor = Math.min(screen.width / w, screen.height / h);
         w = Math.round(w * factor);
         h = Math.round(h * factor);
      }
      if (Browser.resizeCanvas) {
        if (canvas.width  != w) canvas.width  = w;
        if (canvas.height != h) canvas.height = h;
        if (typeof canvas.style != 'undefined') {
          canvas.style.removeProperty( "width");
          canvas.style.removeProperty("height");
        }
      } else {
        if (canvas.width  != wNative) canvas.width  = wNative;
        if (canvas.height != hNative) canvas.height = hNative;
        if (typeof canvas.style != 'undefined') {
          if (w != wNative || h != hNative) {
            canvas.style.setProperty( "width", w + "px", "important");
            canvas.style.setProperty("height", h + "px", "important");
          } else {
            canvas.style.removeProperty( "width");
            canvas.style.removeProperty("height");
          }
        }
      }
    },

    wgetRequests: {},
    nextWgetRequestHandle: 0,

    getNextWgetRequestHandle: function() {
      var handle = Browser.nextWgetRequestHandle;
      Browser.nextWgetRequestHandle++;
      return handle;
    }
  },

  emscripten_async_wget__deps: ['$PATH'],
  emscripten_async_wget__proxy: 'sync',
  emscripten_async_wget__sig: 'viiii',
  emscripten_async_wget: function(url, file, onload, onerror) {
    Module['noExitRuntime'] = true;

    var _url = Pointer_stringify(url);
    var _file = Pointer_stringify(file);
    _file = PATH.resolve(FS.cwd(), _file);
    function doCallback(callback) {
      if (callback) {
        var stack = stackSave();
        Module['dynCall_vi'](callback, allocate(intArrayFromString(_file), 'i8', ALLOC_STACK));
        stackRestore(stack);
      }
    }
    var destinationDirectory = PATH.dirname(_file);
    FS.createPreloadedFile(
      destinationDirectory,
      PATH.basename(_file),
      _url, true, true,
      function() {
        doCallback(onload);
      },
      function() {
        doCallback(onerror);
      },
      false, // dontCreateFile
      false, // canOwn
      function() { // preFinish
        // if a file exists there, we overwrite it
        try {
          FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);
      }
    );
  },

  emscripten_async_wget_data__proxy: 'sync',
  emscripten_async_wget_data__sig: 'viiii',
  emscripten_async_wget_data: function(url, arg, onload, onerror) {
    Browser.asyncLoad(Pointer_stringify(url), function(byteArray) {
      var buffer = _malloc(byteArray.length);
      HEAPU8.set(byteArray, buffer);
      Module['dynCall_viii'](onload, arg, buffer, byteArray.length);
      _free(buffer);
    }, function() {
      if (onerror) Module['dynCall_vi'](onerror, arg);
    }, true /* no need for run dependency, this is async but will not do any prepare etc. step */ );
  },

  emscripten_async_wget2__proxy: 'sync',
  emscripten_async_wget2__sig: 'iiiiiiiii',
  emscripten_async_wget2: function(url, file, request, param, arg, onload, onerror, onprogress) {
    Module['noExitRuntime'] = true;

    var _url = Pointer_stringify(url);
    var _file = Pointer_stringify(file);
    _file = PATH.resolve(FS.cwd(), _file);
    var _request = Pointer_stringify(request);
    var _param = Pointer_stringify(param);
    var index = _file.lastIndexOf('/');

    var http = new XMLHttpRequest();
    http.open(_request, _url, true);
    http.responseType = 'arraybuffer';

    var handle = Browser.getNextWgetRequestHandle();

    var destinationDirectory = PATH.dirname(_file);

    // LOAD
    http.onload = function http_onload(e) {
      if (http.status == 200) {
        // if a file exists there, we overwrite it
        try {
          FS.unlink(_file);
        } catch (e) {}
        // if the destination directory does not yet exist, create it
        FS.mkdirTree(destinationDirectory);

        FS.createDataFile( _file.substr(0, index), _file.substr(index + 1), new Uint8Array(http.response), true, true, false);
        if (onload) {
          var stack = stackSave();
          Module['dynCall_viii'](onload, handle, arg, allocate(intArrayFromString(_file), 'i8', ALLOC_STACK));
          stackRestore(stack);
        }
      } else {
        if (onerror) Module['dynCall_viii'](onerror, handle, arg, http.status);
      }

      delete Browser.wgetRequests[handle];
    };

    // ERROR
    http.onerror = function http_onerror(e) {
      if (onerror) Module['dynCall_viii'](onerror, handle, arg, http.status);
      delete Browser.wgetRequests[handle];
    };

    // PROGRESS
    http.onprogress = function http_onprogress(e) {
      if (e.lengthComputable || (e.lengthComputable === undefined && e.total != 0)) {
        var percentComplete = (e.loaded / e.total)*100;
        if (onprogress) Module['dynCall_viii'](onprogress, handle, arg, percentComplete);
      }
    };

    // ABORT
    http.onabort = function http_onabort(e) {
      delete Browser.wgetRequests[handle];
    };

    if (_request == "POST") {
      //Send the proper header information along with the request
      http.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      http.send(_param);
    } else {
      http.send(null);
    }

    Browser.wgetRequests[handle] = http;

    return handle;
  },

  emscripten_async_wget2_data__proxy: 'sync',
  emscripten_async_wget2_data__sig: 'iiiiiiiii',
  emscripten_async_wget2_data: function(url, request, param, arg, free, onload, onerror, onprogress) {
    var _url = Pointer_stringify(url);
    var _request = Pointer_stringify(request);
    var _param = Pointer_stringify(param);

    var http = new XMLHttpRequest();
    http.open(_request, _url, true);
    http.responseType = 'arraybuffer';

    var handle = Browser.getNextWgetRequestHandle();

    // LOAD
    http.onload = function http_onload(e) {
      if (http.status == 200 || _url.substr(0,4).toLowerCase() != "http") {
        var byteArray = new Uint8Array(http.response);
        var buffer = _malloc(byteArray.length);
        HEAPU8.set(byteArray, buffer);
        if (onload) Module['dynCall_viiii'](onload, handle, arg, buffer, byteArray.length);
        if (free) _free(buffer);
      } else {
        if (onerror) Module['dynCall_viiii'](onerror, handle, arg, http.status, http.statusText);
      }
      delete Browser.wgetRequests[handle];
    };

    // ERROR
    http.onerror = function http_onerror(e) {
      if (onerror) {
        Module['dynCall_viiii'](onerror, handle, arg, http.status, http.statusText);
      }
      delete Browser.wgetRequests[handle];
    };

    // PROGRESS
    http.onprogress = function http_onprogress(e) {
      if (onprogress) Module['dynCall_viiii'](onprogress, handle, arg, e.loaded, e.lengthComputable || e.lengthComputable === undefined ? e.total : 0);
    };

    // ABORT
    http.onabort = function http_onabort(e) {
      delete Browser.wgetRequests[handle];
    };

    if (_request == "POST") {
      //Send the proper header information along with the request
      http.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      http.send(_param);
    } else {
      http.send(null);
    }

    Browser.wgetRequests[handle] = http;

    return handle;
  },

  emscripten_async_wget2_abort__proxy: 'sync',
  emscripten_async_wget2_abort__sig: 'vi',
  emscripten_async_wget2_abort: function(handle) {
    var http = Browser.wgetRequests[handle];
    if (http) {
      http.abort();
    }
  },

  emscripten_run_preload_plugins__deps: ['$PATH'],
  emscripten_run_preload_plugins__proxy: 'sync',
  emscripten_run_preload_plugins__sig: 'iiii',
  emscripten_run_preload_plugins: function(file, onload, onerror) {
    Module['noExitRuntime'] = true;

    var _file = Pointer_stringify(file);
    var data = FS.analyzePath(_file);
    if (!data.exists) return -1;
    FS.createPreloadedFile(
      PATH.dirname(_file),
      PATH.basename(_file),
      new Uint8Array(data.object.contents), true, true,
      function() {
        if (onload) Module['dynCall_vi'](onload, file);
      },
      function() {
        if (onerror) Module['dynCall_vi'](onerror, file);
      },
      true // don'tCreateFile - it's already there
    );
    return 0;
  },

  emscripten_run_preload_plugins_data__proxy: 'sync',
  emscripten_run_preload_plugins_data__sig: 'viiiiii',
  emscripten_run_preload_plugins_data: function(data, size, suffix, arg, onload, onerror) {
    Module['noExitRuntime'] = true;

    var _suffix = Pointer_stringify(suffix);
    if (!Browser.asyncPrepareDataCounter) Browser.asyncPrepareDataCounter = 0;
    var name = 'prepare_data_' + (Browser.asyncPrepareDataCounter++) + '.' + _suffix;
    var lengthAsUTF8 = lengthBytesUTF8(name);
    var cname = _malloc(lengthAsUTF8+1);
    stringToUTF8(name, cname, lengthAsUTF8+1);
    FS.createPreloadedFile(
      '/',
      name,
      {{{ makeHEAPView('U8', 'data', 'data + size') }}},
      true, true,
      function() {
        if (onload) Module['dynCall_vii'](onload, arg, cname);
      },
      function() {
        if (onerror) Module['dynCall_vi'](onerror, arg);
      },
      true // don'tCreateFile - it's already there
    );
  },

  // Callable from pthread, executes in pthread context.
  emscripten_async_run_script__deps: ['emscripten_run_script'],
  emscripten_async_run_script: function(script, millis) {
    Module['noExitRuntime'] = true;

    // TODO: cache these to avoid generating garbage
    Browser.safeSetTimeout(function() {
      _emscripten_run_script(script);
    }, millis);
  },

  // TODO: currently not callable from a pthread, but immediately calls onerror() if not on main thread.
  emscripten_async_load_script: function(url, onload, onerror) {
    onload = getFuncWrapper(onload, 'v');
    onerror = getFuncWrapper(onerror, 'v');

#if USE_PTHREADS
    if (ENVIRONMENT_IS_PTHREAD) {
      console.error('emscripten_async_load_script("' + Pointer_stringify(url) + '") failed, emscripten_async_load_script is currently not available in pthreads!');
      return onerror ? onerror() : undefined;
    }
#endif
    Module['noExitRuntime'] = true;

    assert(runDependencies === 0, 'async_load_script must be run when no other dependencies are active');
    var script = document.createElement('script');
    if (onload) {
      script.onload = function script_onload() {
        if (runDependencies > 0) {
          dependenciesFulfilled = onload;
        } else {
          onload();
        }
      };
    }
    if (onerror) script.onerror = onerror;
    script.src = Pointer_stringify(url);
    document.body.appendChild(script);
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_get_main_loop_timing: function(mode, value) {
    if (mode) {{{ makeSetValue('mode', 0, 'Browser.mainLoop.timingMode', 'i32') }}};
    if (value) {{{ makeSetValue('value', 0, 'Browser.mainLoop.timingValue', 'i32') }}};
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_set_main_loop_timing: function(mode, value) {
    Browser.mainLoop.timingMode = mode;
    Browser.mainLoop.timingValue = value;

    if (!Browser.mainLoop.func) {
#if ASSERTIONS
      console.error('emscripten_set_main_loop_timing: Cannot set timing mode for main loop since a main loop does not exist! Call emscripten_set_main_loop first to set one up.');
#endif
      return 1; // Return non-zero on failure, can't set timing mode when there is no main loop.
    }

    if (mode == 0 /*EM_TIMING_SETTIMEOUT*/) {
      Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_setTimeout() {
        var timeUntilNextTick = Math.max(0, Browser.mainLoop.tickStartTime + value - _emscripten_get_now())|0;
        setTimeout(Browser.mainLoop.runner, timeUntilNextTick); // doing this each time means that on exception, we stop
      };
      Browser.mainLoop.method = 'timeout';
    } else if (mode == 1 /*EM_TIMING_RAF*/) {
      Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_rAF() {
        Browser.requestAnimationFrame(Browser.mainLoop.runner);
      };
      Browser.mainLoop.method = 'rAF';
    } else if (mode == 2 /*EM_TIMING_SETIMMEDIATE*/) {
      if (typeof setImmediate === 'undefined') {
        // Emulate setImmediate. (note: not a complete polyfill, we don't emulate clearImmediate() to keep code size to minimum, since not needed)
        var setImmediates = [];
        var emscriptenMainLoopMessageId = 'setimmediate';
        function Browser_setImmediate_messageHandler(event) {
          // When called in current thread or Worker, the main loop ID is structured slightly different to accommodate for --proxy-to-worker runtime listening to Worker events,
          // so check for both cases.
          if (event.data === emscriptenMainLoopMessageId || event.data.target === emscriptenMainLoopMessageId) {
            event.stopPropagation();
            setImmediates.shift()();
          }
        }
        addEventListener("message", Browser_setImmediate_messageHandler, true);
        setImmediate = function Browser_emulated_setImmediate(func) {
          setImmediates.push(func);
          if (ENVIRONMENT_IS_WORKER) {
            if (Module['setImmediates'] === undefined) Module['setImmediates'] = [];
            Module['setImmediates'].push(func);
            postMessage({target: emscriptenMainLoopMessageId}); // In --proxy-to-worker, route the message via proxyClient.js
          } else postMessage(emscriptenMainLoopMessageId, "*"); // On the main thread, can just send the message to itself.
        }
      }
      Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_setImmediate() {
        setImmediate(Browser.mainLoop.runner);
      };
      Browser.mainLoop.method = 'immediate';
    }
    return 0;
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_set_main_loop__deps: ['emscripten_set_main_loop_timing', 'emscripten_get_now'],
  emscripten_set_main_loop: function(func, fps, simulateInfiniteLoop, arg, noSetTiming) {
    Module['noExitRuntime'] = true;

    assert(!Browser.mainLoop.func, 'emscripten_set_main_loop: there can only be one main loop function at once: call emscripten_cancel_main_loop to cancel the previous one before setting a new one with different parameters.');

    Browser.mainLoop.func = func;
    Browser.mainLoop.arg = arg;

    var browserIterationFunc;
    if (typeof arg !== 'undefined') {
      browserIterationFunc = function() {
        Module['dynCall_vi'](func, arg);
      };
    } else {
      browserIterationFunc = function() {
        Module['dynCall_v'](func);
      };
    }

    var thisMainLoopId = Browser.mainLoop.currentlyRunningMainloop;

    Browser.mainLoop.runner = function Browser_mainLoop_runner() {
      if (ABORT) return;
      if (Browser.mainLoop.queue.length > 0) {
        var start = Date.now();
        var blocker = Browser.mainLoop.queue.shift();
        blocker.func(blocker.arg);
        if (Browser.mainLoop.remainingBlockers) {
          var remaining = Browser.mainLoop.remainingBlockers;
          var next = remaining%1 == 0 ? remaining-1 : Math.floor(remaining);
          if (blocker.counted) {
            Browser.mainLoop.remainingBlockers = next;
          } else {
            // not counted, but move the progress along a tiny bit
            next = next + 0.5; // do not steal all the next one's progress
            Browser.mainLoop.remainingBlockers = (8*remaining + next)/9;
          }
        }
        console.log('main loop blocker "' + blocker.name + '" took ' + (Date.now() - start) + ' ms'); //, left: ' + Browser.mainLoop.remainingBlockers);
        Browser.mainLoop.updateStatus();

        // catches pause/resume main loop from blocker execution
        if (thisMainLoopId < Browser.mainLoop.currentlyRunningMainloop) return;

        setTimeout(Browser.mainLoop.runner, 0);
        return;
      }

      // catch pauses from non-main loop sources
      if (thisMainLoopId < Browser.mainLoop.currentlyRunningMainloop) return;

      // Implement very basic swap interval control
      Browser.mainLoop.currentFrameNumber = Browser.mainLoop.currentFrameNumber + 1 | 0;
      if (Browser.mainLoop.timingMode == 1/*EM_TIMING_RAF*/ && Browser.mainLoop.timingValue > 1 && Browser.mainLoop.currentFrameNumber % Browser.mainLoop.timingValue != 0) {
        // Not the scheduled time to render this frame - skip.
        Browser.mainLoop.scheduler();
        return;
      } else if (Browser.mainLoop.timingMode == 0/*EM_TIMING_SETTIMEOUT*/) {
        Browser.mainLoop.tickStartTime = _emscripten_get_now();
      }

      // Signal GL rendering layer that processing of a new frame is about to start. This helps it optimize
      // VBO double-buffering and reduce GPU stalls.
#if USES_GL_EMULATION
      GL.newRenderingFrameStarted();
#endif

#if OFFSCREENCANVAS_SUPPORT
      // If the current GL context is an OffscreenCanvas, but it was initialized with implicit swap mode, perform the swap
      // in behalf of the user.
      if (typeof GL !== 'undefined' && GL.currentContext && !GL.currentContext.attributes.explicitSwapControl && GL.currentContext.GLctx.commit) {
        GL.currentContext.GLctx.commit();
      }
#endif

      if (Browser.mainLoop.method === 'timeout' && Module.ctx) {
        Browser.mainLoop.method = ''; // just warn once per call to set main loop
      }

      Browser.mainLoop.runIter(browserIterationFunc);

#if STACK_OVERFLOW_CHECK
      checkStackCookie();
#endif

      // catch pauses from the main loop itself
      if (thisMainLoopId < Browser.mainLoop.currentlyRunningMainloop) return;

      // Queue new audio data. This is important to be right after the main loop invocation, so that we will immediately be able
      // to queue the newest produced audio samples.
      // TODO: Consider adding pre- and post- rAF callbacks so that GL.newRenderingFrameStarted() and SDL.audio.queueNewAudioData()
      //       do not need to be hardcoded into this function, but can be more generic.
      if (typeof SDL === 'object' && SDL.audio && SDL.audio.queueNewAudioData) SDL.audio.queueNewAudioData();

      Browser.mainLoop.scheduler();
    }

    if (!noSetTiming) {
      if (fps && fps > 0) _emscripten_set_main_loop_timing(0/*EM_TIMING_SETTIMEOUT*/, 1000.0 / fps);
      else _emscripten_set_main_loop_timing(1/*EM_TIMING_RAF*/, 1); // Do rAF by rendering each frame (no decimating)

      Browser.mainLoop.scheduler();
    }

    if (simulateInfiniteLoop) {
      throw 'SimulateInfiniteLoop';
    }
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_set_main_loop_arg__deps: ['emscripten_set_main_loop'],
  emscripten_set_main_loop_arg: function(func, arg, fps, simulateInfiniteLoop) {
    _emscripten_set_main_loop(func, fps, simulateInfiniteLoop, arg);
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_cancel_main_loop: function() {
    Browser.mainLoop.pause();
    Browser.mainLoop.func = null;
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_pause_main_loop: function() {
    Browser.mainLoop.pause();
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_resume_main_loop: function() {
    Browser.mainLoop.resume();
  },

  // Runs natively in pthread, no __proxy needed.
  _emscripten_push_main_loop_blocker: function(func, arg, name) {
    Browser.mainLoop.queue.push({ func: function() {
      Module['dynCall_vi'](func, arg);
    }, name: Pointer_stringify(name), counted: true });
    Browser.mainLoop.updateStatus();
  },

  // Runs natively in pthread, no __proxy needed.
  _emscripten_push_uncounted_main_loop_blocker: function(func, arg, name) {
    Browser.mainLoop.queue.push({ func: function() {
      Module['dynCall_vi'](func, arg);
    }, name: Pointer_stringify(name), counted: false });
    Browser.mainLoop.updateStatus();
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_set_main_loop_expected_blockers: function(num) {
    Browser.mainLoop.expectedBlockers = num;
    Browser.mainLoop.remainingBlockers = num;
    Browser.mainLoop.updateStatus();
  },

  // Runs natively in pthread, no __proxy needed.
  emscripten_async_call: function(func, arg, millis) {
    Module['noExitRuntime'] = true;

    function wrapper() {
      getFuncWrapper(func, 'vi')(arg);
    }

    if (millis >= 0) {
      Browser.safeSetTimeout(wrapper, millis);
    } else {
      Browser.safeRequestAnimationFrame(wrapper);
    }
  },

  // Callable in pthread without __proxy needed.
  emscripten_exit_with_live_runtime: function() {
    Module['noExitRuntime'] = true;
    throw 'SimulateInfiniteLoop';
  },

  emscripten_force_exit__proxy: 'sync',
  emscripten_force_exit__sig: 'vi',
  emscripten_force_exit: function(status) {
#if NO_EXIT_RUNTIME
#if ASSERTIONS
    warnOnce('emscripten_force_exit cannot actually shut down the runtime, as the build has NO_EXIT_RUNTIME set');
#endif
#endif
    Module['noExitRuntime'] = false;
    exit(status);
  },

  emscripten_get_device_pixel_ratio__proxy: 'sync',
  emscripten_get_device_pixel_ratio__sig: 'd',
  emscripten_get_device_pixel_ratio: function() {
    return window.devicePixelRatio || 1.0;
  },

  emscripten_hide_mouse__proxy: 'sync',
  emscripten_hide_mouse__sig: 'v',
  emscripten_hide_mouse: function() {
    var styleSheet = document.styleSheets[0];
    var rules = styleSheet.cssRules;
    for (var i = 0; i < rules.length; i++) {
      if (rules[i].cssText.substr(0, 6) == 'canvas') {
        styleSheet.deleteRule(i);
        i--;
      }
    }
    styleSheet.insertRule('canvas.emscripten { border: 1px solid black; cursor: none; }', 0);
  },

  emscripten_set_canvas_size__proxy: 'sync',
  emscripten_set_canvas_size__sig: 'vii',
  emscripten_set_canvas_size: function(width, height) {
    Browser.setCanvasSize(width, height);
  },

  emscripten_get_canvas_size__proxy: 'sync',
  emscripten_get_canvas_size__sig: 'viii',
  emscripten_get_canvas_size: function(width, height, isFullscreen) {
    var canvas = Module['canvas'];
    {{{ makeSetValue('width', '0', 'canvas.width', 'i32') }}};
    {{{ makeSetValue('height', '0', 'canvas.height', 'i32') }}};
    {{{ makeSetValue('isFullscreen', '0', 'Browser.isFullscreen ? 1 : 0', 'i32') }}};
  },

  // To avoid creating worker parent->child chains, always proxies to execute on the main thread.
  emscripten_create_worker__proxy: 'sync',
  emscripten_create_worker__sig: 'ii',
  emscripten_create_worker: function(url) {
    url = Pointer_stringify(url);
    var id = Browser.workers.length;
    var info = {
      worker: new Worker(url),
      callbacks: [],
      awaited: 0,
      buffer: 0,
      bufferSize: 0
    };
    info.worker.onmessage = function info_worker_onmessage(msg) {
      if (ABORT) return;
      var info = Browser.workers[id];
      if (!info) return; // worker was destroyed meanwhile
      var callbackId = msg.data['callbackId'];
      var callbackInfo = info.callbacks[callbackId];
      if (!callbackInfo) return; // no callback or callback removed meanwhile
      // Don't trash our callback state if we expect additional calls.
      if (msg.data['finalResponse']) {
        info.awaited--;
        info.callbacks[callbackId] = null; // TODO: reuse callbackIds, compress this
      }
      var data = msg.data['data'];
      if (data) {
        if (!data.byteLength) data = new Uint8Array(data);
        if (!info.buffer || info.bufferSize < data.length) {
          if (info.buffer) _free(info.buffer);
          info.bufferSize = data.length;
          info.buffer = _malloc(data.length);
        }
        HEAPU8.set(data, info.buffer);
        callbackInfo.func(info.buffer, data.length, callbackInfo.arg);
      } else {
        callbackInfo.func(0, 0, callbackInfo.arg);
      }
    };
    Browser.workers.push(info);
    return id;
  },

  emscripten_destroy_worker__proxy: 'sync',
  emscripten_destroy_worker__sig: 'vi',
  emscripten_destroy_worker: function(id) {
    var info = Browser.workers[id];
    info.worker.terminate();
    if (info.buffer) _free(info.buffer);
    Browser.workers[id] = null;
  },

  emscripten_call_worker__proxy: 'sync',
  emscripten_call_worker__sig: 'viiiiii',
  emscripten_call_worker: function(id, funcName, data, size, callback, arg) {
    Module['noExitRuntime'] = true; // should we only do this if there is a callback?

    funcName = Pointer_stringify(funcName);
    var info = Browser.workers[id];
    var callbackId = -1;
    if (callback) {
      callbackId = info.callbacks.length;
      info.callbacks.push({
        func: getFuncWrapper(callback, 'viii'),
        arg: arg
      });
      info.awaited++;
    }
    var transferObject = {
      'funcName': funcName,
      'callbackId': callbackId,
      'data': data ? new Uint8Array({{{ makeHEAPView('U8', 'data', 'data + size') }}}) : 0
    };
    if (data) {
      info.worker.postMessage(transferObject, [transferObject.data.buffer]);
    } else {
      info.worker.postMessage(transferObject);
    }
  },

  emscripten_worker_respond_provisionally__proxy: 'sync',
  emscripten_worker_respond_provisionally__sig: 'vii',
  emscripten_worker_respond_provisionally: function(data, size) {
    if (workerResponded) throw 'already responded with final response!';
    var transferObject = {
      'callbackId': workerCallbackId,
      'finalResponse': false,
      'data': data ? new Uint8Array({{{ makeHEAPView('U8', 'data', 'data + size') }}}) : 0
    };
    if (data) {
      postMessage(transferObject, [transferObject.data.buffer]);
    } else {
      postMessage(transferObject);
    }
  },

  emscripten_worker_respond__proxy: 'sync',
  emscripten_worker_respond__sig: 'vii',
  emscripten_worker_respond: function(data, size) {
    if (workerResponded) throw 'already responded with final response!';
    workerResponded = true;
    var transferObject = {
      'callbackId': workerCallbackId,
      'finalResponse': true,
      'data': data ? new Uint8Array({{{ makeHEAPView('U8', 'data', 'data + size') }}}) : 0
    };
    if (data) {
      postMessage(transferObject, [transferObject.data.buffer]);
    } else {
      postMessage(transferObject);
    }
  },

  emscripten_get_worker_queue_size__proxy: 'sync',
  emscripten_get_worker_queue_size__sig: 'i',
  emscripten_get_worker_queue_size: function(id) {
    var info = Browser.workers[id];
    if (!info) return -1;
    return info.awaited;
  },

  emscripten_get_preloaded_image_data__deps: ['$PATH'],
  emscripten_get_preloaded_image_data__proxy: 'sync',
  emscripten_get_preloaded_image_data__sig: 'iiii',
  emscripten_get_preloaded_image_data: function(path, w, h) {
    if (typeof path === "number") {
      path = Pointer_stringify(path);
    }

    path = PATH.resolve(path);

    var canvas = Module["preloadedImages"][path];
    if (canvas) {
      var ctx = canvas.getContext("2d");
      var image = ctx.getImageData(0, 0, canvas.width, canvas.height);
      var buf = _malloc(canvas.width * canvas.height * 4);

      HEAPU8.set(image.data, buf);

      {{{ makeSetValue('w', '0', 'canvas.width', 'i32') }}};
      {{{ makeSetValue('h', '0', 'canvas.height', 'i32') }}};
      return buf;
    }

    return 0;
  },

  emscripten_get_preloaded_image_data_from_FILE__deps: ['emscripten_get_preloaded_image_data'],
  emscripten_get_preloaded_image_data_from_FILE__proxy: 'sync',
  emscripten_get_preloaded_image_data_from_FILE__sig: 'iiii',
  emscripten_get_preloaded_image_data_from_FILE: function(file, w, h) {
    var fd = Module['_fileno'](file);
    var stream = FS.getStream(fd);
    if (stream) {
      return _emscripten_get_preloaded_image_data(stream.path, w, h);
    }

    return 0;
  }
};

autoAddDeps(LibraryBrowser, '$Browser');

mergeInto(LibraryManager.library, LibraryBrowser);

/* Useful stuff for browser debugging

function slowLog(label, text) {
  if (!slowLog.labels) slowLog.labels = {};
  if (!slowLog.labels[label]) slowLog.labels[label] = 0;
  var now = Date.now();
  if (now - slowLog.labels[label] > 1000) {
    out(label + ': ' + text);
    slowLog.labels[label] = now;
  }
}

*/
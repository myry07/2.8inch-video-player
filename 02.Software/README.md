encod mjpeg
288*240
```
ffmpeg -i input.mp4 -vf "fps=30,scale=-1:240:flags=lanczos,crop=288:in_h:(in_w-288)/2:0" -q:v 11 288_30fps.mjpeg
```

320*240
```
ffmpeg -i input.mp4 -vf "fps=24,scale=320:240:flags=lanczos" -q:v 9 320_240.mjpeg
```

endcod aac
```
ffmpeg -i input.mp4 -ar 44100 -ac 1 -ab 24k -filter:a loudnorm -filter:a "volume=5dB" 44100.aac
```
endcod mp3
```
ffmpeg -i input.mp4 -ar 44100 -ac 1 -q:a 9 output.mp3
```
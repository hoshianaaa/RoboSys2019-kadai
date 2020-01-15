# RoboSys2019-kadai
+ ロボットシステム学2019の課題  
  
## 実施内容  
+ RaspberryPiのPWMを使用し,ステッピングモータを駆動  
+ PWMはレジスタから設定  
  
## 実験結果  
+ [YouTube](https://www.youtube.com/watch?v=UzrsNDXSdO8&feature=youtu.be)  
## 環境  
+ Raspberry Pi 3 Model B Plus Rev 1.3 (BCM2835)  
+ Raspbian GNU/Linux 9.6 (stretch) armv7l  
## 参考  
+ [参考プログラム](https://github.com/rt-net/RaspberryPiMouse)
## 実行方法  
bash reload.bash  
echo {周波数} > /dev/driver0


float x;
float y;
float lr;

void setup(){
  size(300,300);
  background(255,255,255);
  frameRate(2000);
  x = 0;
  y = 10; 
  lr = 1;
}
void draw(){
  //x++;
  x += lr;
  colorMode(RGB,256);
  point(x, y);
 
  if(x>300){
     stroke(random(255), random(255), random(255));
     y = random(300);
     lr = -1;
  }

  if(x<0){
     stroke(random(255), random(255), random(255));
     y = random(300);
     lr = 1;
  }
  
}

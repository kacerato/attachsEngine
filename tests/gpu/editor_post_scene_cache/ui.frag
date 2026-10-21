#version 450
layout(push_constant) uniform Ui {vec4 rgba;} ui;
layout(location=0) out vec4 color;
void main(){color=ui.rgba;}

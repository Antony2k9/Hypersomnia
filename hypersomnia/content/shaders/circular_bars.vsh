//#extension GL_ARB_explicit_attrib_location : enable
precision mediump int;
precision mediump float;


uniform mat4 projection_matrix;
in vec2 position;
in vec2 texcoord;
in vec4 color;
in vec4 special;

smooth out vec4 theColor;
out vec2 startingAngleVec;
out vec2 endingAngleVec;
out vec2 startingAngleInsideVec;
out vec2 endingAngleInsideVec;
out vec2 theTexcoord;

#define SPECIAL_TO_RADIANS 180.0f*0.0174532925f

void main() 
{
	vec4 output_vert;
	output_vert.x = position.x;		
	output_vert.y = position.y;				
	output_vert.z = 0.0f;						
	output_vert.w = 1.0f;

	startingAngleVec = vec2(cos(special.x * SPECIAL_TO_RADIANS), sin(special.x * SPECIAL_TO_RADIANS));
	endingAngleVec = vec2(cos(special.y * SPECIAL_TO_RADIANS), sin(special.y * SPECIAL_TO_RADIANS));

	startingAngleInsideVec = vec2(cos(special.z * SPECIAL_TO_RADIANS), sin(special.z * SPECIAL_TO_RADIANS));
	endingAngleInsideVec = vec2(cos(special.w * SPECIAL_TO_RADIANS), sin(special.w * SPECIAL_TO_RADIANS));
	
	normalize(startingAngleVec);
	normalize(endingAngleVec);

	gl_Position = projection_matrix*output_vert;
	theColor = color;
	theTexcoord = texcoord;
}
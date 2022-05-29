precision mediump int;
precision mediump float;

smooth in vec4 theColor;
in vec2 theTexcoord;

out vec4 outputColor;

uniform sampler2D basic_texture;
uniform sampler2D light_texture;

const int light_levels = 3;
const int light_step = 255/light_levels;

void main() 
{
	vec2 texcoord = gl_FragCoord.xy;
	texcoord.x /= float(textureSize(light_texture, 0).x);
	texcoord.y /= float(textureSize(light_texture, 0).y);

	vec4 light = texture(light_texture, texcoord);

	float intensity = max(max(light.r, light.g), light.b);
	
	int level = (int(intensity * 255.0) / light_step + light_levels);

	if(level < 4)
		discard;

	intensity = float(
		
		light_step * (level + 3)

		) / 255.0;
	light.rgb *= intensity;

	vec4 pixel = theColor * texture(basic_texture, theTexcoord) * light;
	outputColor = pixel;
}
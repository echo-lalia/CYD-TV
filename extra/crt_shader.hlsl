
//!PARAM scanlines_opacity
//!TYPE float
//!MINIMUM 0
//!MAXIMUM 1
0.2
//!PARAM scanlines_width
//!TYPE float
//!MINIMUM 0
//!MAXIMUM 0.5
0.2


//!PARAM roll
//!TYPE int
//!MINIMUM 0
//!MAXIMUM 1
1

//!PARAM roll_speed
//!DESC Positive values are down, negative are up
//!TYPE float
6.0
//!PARAM roll_size
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 100.0
12.0
//!PARAM roll_variation
//!TYPE float
//!MINIMUM 0.1
//!MAXIMUM 5.0
1.8
//!PARAM distort_intensity
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 0.2
0.0003


//!PARAM static_noise_intensity
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 1.0
0.005


//!PARAM aberration
//!DESC Chromatic aberration, a distortion on each color channel.
//!TYPE float
//!MINIMUM -1.0
//!MAXIMUM 1.0
0.02
//!PARAM brightness
//!DESC When adding scanline gaps and grille the image can get very dark. Brightness tries to compensate for that.
//!TYPE float
1.3
//!PARAM discolor
//!DESC Add a discolor effect simulating a VHS
//!TYPE int
//!MINIMUM 0
//!MAXIMUM 1
1


//!PARAM warp_amount
//!DESC Warp the texture edges simulating the curved glass of a CRT monitor or old TV.
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 5.0
0.1


//!PARAM vignette_intensity
//!DESC Size of the vignette, how far towards the middle it should go.
//!TYPE float
0.9
//!PARAM vignette_opacity
//!TYPE float
//!MINIMUM 0.0
//!MAXIMUM 1.0
0.6






//!HOOK LINEAR
//!BIND HOOKED

#define DEFAULT_FPS 12
#define scanline_color vec3(0.251, 0.157, 0.267)
// resolution controls the size of the scanlines.
#define resolution vec2(80, 60)


// Used by the noise functin to generate a pseudo random value between 0.0 and 1.0
vec2 _random(vec2 uv){
    uv = vec2( dot(uv, vec2(127.1,311.7) ),
               dot(uv, vec2(269.5,183.3) ) );
    return -1.0 + 2.0 * fract(sin(uv) * 43758.5453123);
}

// Generate a Perlin noise used by the distortion effects
float noise(vec2 uv) {
    vec2 uv_index = floor(uv);
    vec2 uv_fract = fract(uv);

    vec2 blur = smoothstep(0.0, 1.0, uv_fract);

    return mix( mix( dot( _random(uv_index + vec2(0.0,0.0) ), uv_fract - vec2(0.0,0.0) ),
                     dot( _random(uv_index + vec2(1.0,0.0) ), uv_fract - vec2(1.0,0.0) ), blur.x),
                mix( dot( _random(uv_index + vec2(0.0,1.0) ), uv_fract - vec2(0.0,1.0) ),
                     dot( _random(uv_index + vec2(1.0,1.0) ), uv_fract - vec2(1.0,1.0) ), blur.x), blur.y) * 0.5 + 0.5;
}

// Takes in the UV and warps the edges, creating the spherized effect
vec2 warp(vec2 uv){
	vec2 delta = uv - 0.5;
	float delta2 = dot(delta.xy, delta.xy);
	float delta4 = delta2 * delta2;
	float delta_offset = delta4 * warp_amount;

	return uv + delta * delta_offset;
}

// Adds a black border to hide stretched pixel created by the warp effect
float border (vec2 uv){
	float radius = min(warp_amount, 0.08);
	radius = max(min(min(abs(radius * 2.0), abs(1.0)), abs(1.0)), 1e-5);
	vec2 abs_uv = abs(uv * 2.0 - 1.0) - vec2(1.0, 1.0) + radius;
	float dist = length(max(vec2(0.0), abs_uv)) / radius;
	float square = smoothstep(0.96, 1.0, dist);
	return clamp(1.0 - square, 0.0, 1.0);
}

// Adds a vignette shadow to the edges of the image
float vignette(vec2 uv){
	uv = uv * (1.0 - uv.xy);
	float vignette = uv.x * uv.y * 15.0;
	return pow(vignette, vignette_intensity * vignette_opacity);
}





vec4 hook()
{
    // This shader is based off of a gdshader I like,
    // so we're emulating a couple of gdshader constants from their mpv counterparts.
    vec2 UV = HOOKED_pos;
    float TIME = float(frame) / DEFAULT_FPS;

	// Move the roll effect left and right (- and +) based on time
	float distort_direction = sin(TIME * roll_speed * 0.61) + sin(TIME * roll_speed * 0.491 - 0.5);
	distort_direction *= distort_intensity;

    vec2 uv = warp(UV); // Warp the uv. uv will be used in most cases instead of UV to keep the warping
	vec2 text_uv = uv;
	vec2 roll_uv = vec2(0.0);
	float time = (roll==1) ? TIME : 0.0;

    // Create the rolling effect.
	float roll_line = 0.0;
	if (roll==1)
	{
		// Create the areas/lines where the texture will be distorted.
		roll_line = smoothstep(0.3, 0.9, sin(uv.y * roll_size - (time * roll_speed) ) );
		// Create more lines of a different size and apply to the first set of lines. This creates a bit of variation.
		roll_line *= roll_line * smoothstep(0.3, 0.9, sin(uv.y * roll_size * roll_variation - (time * roll_speed * roll_variation) ) );
		// Distort the UV where where the lines are
		roll_uv = vec2(( roll_line * distort_direction * (1.-UV.x)), 0.0);
	}

    vec4 text;
	if (roll==1)
	{
		// If roll is true distort the texture with roll_uv. The texture is split up into RGB to
		// make some chromatic aberration. We apply the aberration to the red and green channels according to the aberration parameter
		// and intensify it a bit in the roll distortion.
		text.r = HOOKED_tex(text_uv + roll_uv * 0.8 + vec2(aberration, 0.0) * .1).r;
		text.g = HOOKED_tex(text_uv + roll_uv * 1.2 - vec2(aberration, 0.0) * .1 ).g;
		text.b = HOOKED_tex(text_uv + roll_uv).b;
		text.a = 1.0;
	}
	else
	{
		// If roll is false only apply the aberration without any distorion. The aberration values are very small so the .1 is only
		// to make the slider in the Inspector less sensitive.
		text.r = HOOKED_tex(text_uv + vec2(aberration, 0.0) * .1).r;
		text.g = HOOKED_tex(text_uv - vec2(aberration, 0.0) * .1).g;
		text.b = HOOKED_tex(text_uv).b;
		text.a = 1.0;
	}

	uv = warp(UV);

    // Apply Brightness. Since the scanlines (below) make the image very dark you
	// can compensate by increasing the brightness.
	text.r = clamp(text.r * brightness, 0.0, 1.0);
	text.g = clamp(text.g * brightness, 0.0, 1.0);
	text.b = clamp(text.b * brightness, 0.0, 1.0);

    // Scanlines are the horizontal lines that make up the image on a CRT monitor.
	// Here we are actual setting the black gap between each line, which I guess is not the right definition of the word, but you get the idea
	float scanlines = 0.5;
	if (scanlines_opacity > 0.0)
	{
		// Create lines with sine and apply it to the texture. Smoothstep to allow setting the line size.
		scanlines = smoothstep(scanlines_width, scanlines_width + 0.5, abs(sin(uv.y * (resolution.y * 3.14159265))));
		text.rgb = mix(text.rgb, text.rgb * vec3(scanlines), scanlines_opacity);
	}

    // Apply static noise by generating it over the whole screen
	if (static_noise_intensity > 0.0)
	{
		text.rgb += clamp(_random((ceil(uv * resolution) / resolution) + fract(TIME)).x, 0.0, 1.0) * static_noise_intensity;
	}

    // Apply a black border to hide imperfections caused by the warping.
	// Also apply the vignette
	text.rgb *= border(uv);
	text.rgb *= vignette(uv);

    // Apply discoloration to get a VHS look (lower saturation and higher contrast)
	// You can play with the values below
	float saturation = 0.05;
	// float contrast = 1.0;
	if (discolor==1)
	{
		// Saturation
		vec3 greyscale = vec3(text.r + text.g + text.b) / 3.;
		text.rgb = mix(text.rgb, greyscale, saturation);

		// Contrast
		// float midpoint = pow(0.5, 2.2);
		// text.rgb = (text.rgb - vec3(midpoint)) * contrast + midpoint;
	}

    return text;
}

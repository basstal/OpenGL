#version 330 core
out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

in VS_OUT
{
    vec3 NormalModel;
    vec3 Normal;
    vec2 TexCoords;
    vec3 Position;
    mat4 projection;
    mat4 view;
    mat4 model;
}fs_in;

struct Light {
    vec3 Position;
    vec3 Color;
    float Radius;
    float Linear;
    float Quadratic;
};

uniform Light light;
uniform float shininess;
uniform vec2 windowSize;
uniform vec3 viewPos;

// float near = 0.1;
// float far = 100;

// float clamp_to_frust(float z)
// {
//     return (2.0 * near * far) / (far + near - z * (far - near));
// }

// float LinearizeDepth(float depth) 
// {
//     float z = depth * 2.0 - 1.0; // back to NDC 
//     return clamp_to_frust(z);
// }

// vec2 sphIntersect( vec3 ro,  vec3 rd, vec3 ce, float ra )
// {
//     vec3 oc = ro - ce;
//     float b = dot( oc, rd );
//     float c = dot( oc, oc ) - ra*ra;
//     float h = b*b - c;
//     if( h<0.0 ) return vec2(-1.0); // no intersection
//     h = sqrt( h );
//     return vec2( -b-h, -b+h );
// }

void main()
{
    // mat4 transNDCToModel = inverse(fs_in.view * fs_in.projection * fs_in.model);

    // vec3 tracePosModel = vec3(transNDCToModel * vec4(gl_FragCoord.xy / windowSize * 2.0 - 1.0, gl_FragCoord.z * 2.0 - 1.0, 1.0));
    mat4 inverseModel = inverse(fs_in.model);
    vec3 viewPosModel = vec3(inverseModel * vec4(viewPos, 1.0));
    vec3 V = normalize(fs_in.Position - viewPosModel);
    if (dot(V, fs_in.NormalModel) > 0)
    {
        discard;
    }

    // ** 求视线与球面的交点
    
    vec3 S = viewPosModel;
    float a = pow(length(V), 2);
    float b = 2 * (dot(S, V));
    float c = pow(length(S), 2) - pow(light.Radius, 2);
    float D = pow(b, 2) - 4 * a * c;
    float sqrt_D = sqrt(D);
    float minT = (-b - sqrt_D) / (2 * a);
    float maxT = (-b + sqrt_D) / (2 * a);
    // model coordinate
    vec3 p0 = S + minT * V;
    vec3 p1 = S + maxT * V;
    // vec3 intersection = p1 - p0;
    
    
    vec2 samplerUV = (gl_FragCoord.xy - 0.5) / windowSize;
    // retrieve data from G-buffer
    vec3 FragPos = texture(gPosition, samplerUV).rgb;
    vec3 FragPosNDC = vec3(fs_in.projection * fs_in.view * vec4(FragPos, 1.0));
    mat4 transToNDC = fs_in.projection * fs_in.view * fs_in.model;
    vec3 minP = vec3(transToNDC * vec4(p0, 1.0));
    vec3 maxP = vec3(transToNDC * vec4(p1, 1.0));
    // float len = clamp_to_frust(length(IntersectionNDC));
    // float startDepth = LinearizeDepth(gl_FragCoord.z);
    // FragColor = vec4(viewPos, 1);
    if (FragPosNDC.z < minP.z || FragPosNDC.z > maxP.z)
    {
        discard;
    }
    vec3 Normal = texture(gNormal, samplerUV).rgb;
    vec3 Albedo = texture(gAlbedoSpec, samplerUV).rgb;
    float Specular = texture(gAlbedoSpec, samplerUV).a;
    bool subjects = Normal.x != 0 || Normal.y != 0 || Normal.z != 0;
    vec3 lighting = vec3(0);
    if (subjects)
    {
        lighting = Albedo * 0.1f;
        vec3 viewDir = normalize(viewPos - FragPos);
        float light2FragDist = length(light.Position - FragPos);
        // diffuse
        vec3 lightDir = normalize(light.Position - FragPos);
        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Albedo * light.Color;

        // specular
        vec3 reflectDir = reflect(-lightDir, Normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = Specular * spec * light.Color;

        // attenuation
        float attenuation = 1.0 / (1.0 + light.Linear * light2FragDist + light.Quadratic * light2FragDist * light2FragDist);
        diffuse *= attenuation;
        specular *= attenuation;
        lighting += diffuse + specular;
    }
    FragColor = vec4(lighting, 1.0);
}
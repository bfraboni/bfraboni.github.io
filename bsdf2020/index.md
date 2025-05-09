# BSDF models implementation and references

I started writing these notes before the [Crash Course in BRDF Implementation](https://boksajak.github.io/blog/BRDF) or the [OpenPBR](https://github.com/AcademySoftwareFoundation/OpenPBR) initiative were published so there is definitely some overlap and I recommend taking a look at them. I intend to finish this write-up at some point but it is currently a work in progress, hence some parts are very incomplete.

## Introduction

This document cover some mathematical and implementation details for various classic bidirectional scattering distribution functions (BSDF) in a path tracer, such as :
- Diffuse models 
    * Lambert
    * Rough diffuse isotropic / anisotropic (GGX based) 
- Conductors / Tinted conductors  
- Dielectrics  
- Microfacet distributions
    * Isotropic GGX
    * Anisotropic GGX
- Rough conductors isotropic / anisotropic (GGX based) 
- Rough dielectrics isotropic / anisotropic (GGX based) 
- Rough plastic isotropic / anisotropic (GGX based) 
- Mix BSDF

<!-- * Oren-Nayar rough diffuse model -->
<!-- * Diffuse transmission -->

## Geometric configuration

A BSDF is a function that returns the amount of light scattered from an incoming direction $\omega_i$ to an outgoing direction $\omega_o$ at a point $p$. Physics-based BSDF should satisfy three main properties:
1. Positivity: $f(\omega_o, \omega_i) \geq 0$
2. Reciprocity: $f(\omega_o, \omega_i) = f(\omega_i, \omega_o)$
3. Energy conservation: $\int_{\Omega_i} f(\omega_o, \omega_i) \cos \theta_i \mathrm{d}\omega_i \leq 1$ 

<center>
    <img style="width: 50%;" src="data/geometric_configuration.png">
</center>

We take by convention that:
- All directions are unit vectors pointing outwards in a local orthonormal basis $(\omega_g,\omega_t,\omega_b)$ centered at the surface point $p$ (unless otherwise stated).
- The surface normal $\omega_g$ is always pointing outside the surface and is oriented Z-up. A light direction $\omega_i$ in the same hemisphere as the normal is entering the object, exiting otherwise.  
- For isotropic materials the local basis can be constructed using an arbitrary tangent vector $\omega_t$ without consequences (e.g. [#Duff17]), or using the following approach.
- For anisotropic materials the tangent vector $\omega_t$ of the basis have to be continuous over the surface. A classic approach to construct a continuous tangent vector is to take the derivative of the position w.r.t the surface parameterization $(u,v)$: $$\omega_t = \text{normalize}\left(\frac{\partial p}{\partial u}\right) \qquad \text{or} \qquad \omega_t = \text{normalize}\left(\frac{\partial p}{\partial v}\right)$$

Additionally, the tangent vector can be rotated around the normal by an angle $\theta$ to change the anisotropic direction: $$\omega_t = \text{rotate}\left(\text{normalize}\left(\frac{\partial p}{\partial u}\right), \omega_g, \theta\right)$$ Several shape parameterizations and the derivatives of position and normal are derived in PBRT [Pharr:2018](#Pharr2018), Chapter 3: Shapes.

Notation | Definition
---|---
$p$ | surface point
$\omega_i$ | incident direction (towards the light)
$\omega_o$ | outgoing direction (towards the eye)
$\omega_m$ | microfacet normal 
$\omega_g$ | geometric normal
$\omega_t$ | tangent vector 
$\omega_b$ | bitangent vector 
$(\omega_g,\omega_t,\omega_b)$ | local orthonormal basis 
<!-- $G_1(\alpha_x, \alpha_y, \omega_h, \omega)$ | Smith masking function -->
<!-- $G_2(\alpha_x, \alpha_y, \omega_h, \omega_i, \omega_o)$ | Smith masking-shadowing function -->
Note that we chose to use the light natural propagation direction to fix the incident / outgoing convention.

    
# C++ interface

In a path tracer, we must be able to evaluate the BSDF at any point $p$ and for any couple of incident and outgoing direction. We compute the amount of scattered light from a point $p$ to an outgoing direction by integrating a set of incident directions . Hence we need a method to sample directions    

<!-- 
0/ il faut pouvoir evaluer la brdf pour les directions camera, source, et qu'une parametrisation de la surface est necessaire pour les surfaces anisotropiques, et pour les perturbations de la normale,

1/ il faut integrer la luminance sur un ensemble de directions incidentes, donc pour sampler une direction vers la source, il faut une methode wsource= sample( wcamera )

2/ il faut aussi pouvoir evaluer la pdf d'une direction quelconque pour utiliser du mis pour l'eclairage direct et choisir dans quelle unite evaluer cette densite : sur les aires, sur les directions...

3/ verifier que l'implem est coherente : white furnace, c'est pour verifier la conservation d'energie, mais il faut aussi verifier que la brdf est reciproque, et que les fonctions sample() et pdf() sont coherentes, et permettent par exemple de calculer l'albedo de la brdf pour une direction camera.

4/ dans la lignee des fonctions de tests, tu peux aussi ajouter une fonction qui calcule une image d'un lobe pour une direction camera.
 -->

The following code snippet describes the common BSDF interface in a path tracer.

We will derive from it all BSDF described in this document. Its main methods are:
- **eval** that returns the BSDF value of a given incident direction $\omega_i$ when the outgoing direction is $\omega_o$ and the texture coordinates are $(u,v)$.
- **pdf** that returns the probability density of a given incident direction $\omega_i$, when the outgoing direction is $\omega_o$ and the texture coordinates are $(u,v)$.
- **sample** that returns a sampled incident direction $\omega_i$ using random samples from the sampler $sampler$, when the outgoing direction is $\omega_o$ and the texture coordinates are $(u,v)$. 
N.B Not all the BSDF sampling methods will consume the same amount of random samples.
However this can lead to dimension mismatching with QMC samplers and inconsistent results.
In some cases, ones would prefer to send a random triplet (or n-uplet) as a parameter, ensuring that for every BSDF samples the same amount of random numbers are passed as argument even if they are not used.

```c++
class Bsdf
{
public:
    Bsdf(){}
    Bsdf(Texture* albedo, BsdfType type = NULL) : m_albedo(albedo), m_type(type) {}
    virtual ~Bsdf() {}
    virtual Color eval(const Vector& wi, const Vector& wo, const vec2& uv) const = 0;
    virtual float pdf(const Vector& wi, const Vector& wo, const vec2& uv) const = 0;
    virtual BsdfSample sample(Sampler& sampler, const Vector& wo, const vec2& uv) const = 0;
    Texture* m_albedo;
    BsdfType m_type;
};
```
We assume there exists a `Texture` class which implements the operator: 
```c++
virtual Color operator() ( const vec2& uv ) const = 0;
```
to access surface properties such as albedo, roughness, anisotropy, etc at the specified coordinates $(u,v)$. 
`BsdfSample` is a small 32-bytes structure containing all required informations of a BSDF sample.
```c++
struct BsdfSample
{
    Vector wi;    // the sampled direction
    float pdf;    // the pdf of the direction
    Color weight; // the associated sample weight (bsdf * cos / pdf)
};

```
Using a dedicated structure allow to return any additional information, such as the lobe that has been sampled for multi-lobe BSDF.
`BsdfType` enumerates the different combinations of BSDF lobes that can be used. 
This is useful to define boolean test functions to retrieve bsdf properties.
```c++
enum BsdfType
{
    NULL                 = 0,
    GLOSSYREFLECTION     = (1 << 0),
    GLOSSYTRANSMISSION   = (1 << 1),
    DIFFUSEREFLECTION    = (1 << 2),
    DIFFUSETRANSMISSION  = (1 << 3),
    SPECULARREFLECTION   = (1 << 4),
    SPECULARTRANSMISSION = (1 << 5),
    ANISOTROPIC          = (1 << 6),
    PHASE                = (1 << 7),
    FORWARD              = (1 << 8),
    BACKWARD             = (1 << 9),
    GLOSSY               = GLOSSYREFLECTION | GLOSSYTRANSMISSION,
    DIFFUSE              = DIFFUSEREFLECTION | DIFFUSETRANSMISSION,
    SPECULAR             = SPECULARREFLECTION | SPECULARTRANSMISSION,
    TRANSMISSIVE         = GLOSSYTRANSMISSION | DIFFUSETRANSMISSION | SPECULARTRANSMISSION,
    REFLECTIVE           = GLOSSYREFLECTION | DIFFUSEREFLECTION | SPECULARREFLECTION,
};

class Bsdf
{
public:
    
    ...
    
    virtual bool hasDiffuse() const { return (m_type & DIFFUSE) != 0; }
    virtual bool hasGlossy() const { return (m_type & GLOSSY) != 0; }
    virtual bool hasSpecular() const { return (m_type & SPECULAR) != 0; }
    virtual bool hasForward() const { return (m_type & FORWARD) != 0; }
    virtual bool hasBackward() const { return (m_type & BACKWARD) != 0; }
    virtual bool isPhase() const { return (m_type & PHASE) != 0; }
    virtual bool isPureDiffuse() const { return m_type != 0 && (m_type & ~DIFFUSE) == 0; }
    virtual bool isPureGlossy() const { return m_type != 0 && (m_type & ~GLOSSY) == 0; }
    virtual bool isPureSpecular() const { return m_type != 0 && (m_type & ~SPECULAR) == 0; }
    virtual bool isForward() const { return m_type != 0 && (m_type & ~FORWARD) == 0; }
    virtual bool isBackward() const { return m_type != 0 && (m_type & ~BACKWARD) == 0; }
    virtual bool isTransmissive() const { return (m_type & TRANSMISSIVE) != 0; }
    virtual bool isAnisotropic() const { return (m_type & ANISOTROPIC) != 0; }
    virtual bool isDelta() const { return isPureSpecular(); }           
    virtual bool hasDelta() const { return hasSpecular(); }           
};
``` 
One last method to add is a test function to ensure that the BSDF is normalized, which is super useful when implementing BSDF models. This test is called the *white furnace test*, see [#Heitz14a] Section 3.5 for further details.
```c++
class Bsdf
{
public:

    ...

    virtual void whiteFurnaceTest() const = 0;
}
```

## Lambert model

<!-- > "First of the 3-models-to-start-with" -->
The Lambert model assumes that the object surface is perfectly flat and that the light is equally reflected in all directions. The amount of reflected light is constant over the hemisphere and is independent of the eye direction:   

$$
    \rho(\omega_i, \omega_o) = \frac{k}{\pi}
$$

Here $k$ is the surface albedo at the position $p$. The amount of light reflected towards a direction $\omega_o$ from a surface point $p$ writes:

$$
    L_r(p, \omega_o) = \int_\Omega L_i(p, \omega_i) \rho(\omega_i, \omega_o) \cos \theta_i \mathrm{d}\omega_i
$$

Estimating this integral requires sampling directions in the hemisphere around $p$. The integrand is weighted by a cosine term, and good importance sampling is achieved when the sampling distribution is proportional to the integrand. Hence, we can use a cosine-weighted distribution to sample direction with $\text{pdf} \propto \cos \theta_i$. The following code samples a cosine-weighted distribution of directions in the hemisphere given two uniform numbers.

```c++
Vector SampleCosineHemisphere( const float u1, const float u2 )
{
    float phi = 2.f * (float) M_PI * u1;
    float cos_theta = std::sqrt(u2);
    float sin_theta = std::sqrt(1.f - std::min(1.f, cos_theta * cos_theta));
    return Vector(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);
}
```
Finally the code for the Lambert BSDF model is:
```c++
class BsdfLambert : public Bsdf
{
public:
    BsdfLambert(Texture* albedo) : Bsdf(albedo, DIFFUSEREFLECTION) {}
    
    Color eval( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return (*m_albedo)(uv) * INV_PI;
    }
    
    float pdf( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return std::max(0.f, wi.z) * INV_PI;
    }

    BsdfSample sample( Sampler& sampler, const Vector& wo, const vec2& uv )
    {   
        BsdfSample bs;
        float u = sampler.uniform(), v = sampler.uniform();
        bs.wi = SampleCosineHemisphere(u, v);
        bs.pdf = std::max(0.f, bs.wi.z) * INV_PI;
        bs.weight = (*m_albedo)(uv);
        return bs;
    }
};
```
This models results in a matte finish with a slight falloff that darken edges. However, there is no control over the falloff with this model and it does not reproduce accurately the behavior of matte materials which tends to look more flat. This can be addressed by using a rough diffuse model such as the Oren-Nayar model [#Oren94] or the stochastic GGX based rough diffuse model by Eric heitz [#Heitz15] which supports anisotropy.

Constant albedo - Environment lightning  | Textured albedo - Environment lightning 
---|---
![](data/test_bsdf_sphere_lambert.png)|![](data/test_bsdf_sphere_lambert_texture.png)

Additional Lambertian diffuse models can be derived, such as Lambert transmission which is super simple and gives nice results for scattering through foliage.

## 100% reflective BSDF

The second model is a 100% reflective material. Although this model is not physically based, it gives pleasant results and is simple to implement. In addition it is a good introduction to understand the reflection law. This model assumes that the surface is perfectly flat, and the light is reflected in a unique direction symmetric w.r.t the normal of the surface. The reflection ray can be computed using the *Snell-Descartes reflection law*:

$$
    \omega_i = 2 \langle \omega_g, \omega_o \rangle \omega_g - \omega_o
$$

If the geometric normal is aligned with the Z axis, $\omega_g = \left(0,0,1\right)$, and the eye vector $\omega_o$ is pointing outwards, the reflection simplifies to:

$$
    \omega_i = \left( -\omega_o.x, -\omega_o.y, \omega_o.z \right)
$$

We can implement both as utility functions in the code:

```c++
// arbitrary normal vector w
static Vector Reflect(const Vector& wo, const Vector& w)
{
    return normalize(2.f * dot(wo, w) * w - wo);
}

// assumes w = Vector(0,0,1)
static Vector Reflect(const Vector& wo)
{
    return Vector(-wo.x, -wo.y, wo.z);
}

```

![Geometric configuration TODO: remake image](https://upload.wikimedia.org/wikipedia/commons/thumb/9/91/Reflexion_fr.png/300px-Reflexion_fr.png)

<!-- The BSDF value is always 1, $\rho(\omega_i, \omega_o) = 1$.  -->
The associated distribution is a Dirac and the pdf of a sampled direction is also 1, since for a given outgoing direction a unique incident direction exists. The **eval** and **pdf** methods can be defined in two ways. A first approach is to return 0 for any direction passed as argument. This follows the principle that a Dirac direction can only be found through the `Bsdf::sample` method and that evaluating the pdf or the function for an incident and outgoing direction does not make sense. A second more flexible approach, consists in returning 1 when the directions $\omega_i$ and $\omega_o$ follow a user-defined constraint of reflection, 0 otherwise (requiring an additional utility function). Finally the complete class writes:

```c++
class BsdfReflective : public Bsdf
{
public:
    BsdfReflective(Texture* albedo) : Bsdf(albedo, SPECULARREFLECTION){}
    
    Color eval( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return Black();
    }
    
    float pdf( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return 0;
    }

    BsdfSample sample( Sampler& sampler, const Vector& wo, const vec2& uv ) const
    {   
        BsdfSample bs:
        bs.wi = Vector(-wo.x, -wo.y, wo.z);
        bs.pdf = 1;
        bs.weight = (*m_albedo)(uv);
        return bs;
    }
};
```
The alternative flexible version with the utility function `isReflection` writes:

```c++
static bool isReflection(const Vector& wi, const Vector& wo)
{
    return std::abs(wi.z*wo.z - wi.x*wo.x - wi.y*wo.y - 1.0f) < 1e-4f;
}

class BsdfReflectiveFlexible : public Bsdf
{
public:
    BsdfReflectiveFlexible(Texture* albedo) : Bsdf(albedo, SPECULARREFLECTION){}
    
    Color eval( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        // N.B. The classic path tracing algorithm performs an integral over 
        // the hemisphere / shape of the BSDF. For that reason the path throughput 
        // is multiplied with an extra cosine term in the main loop.
        // In order not to create a particular case in the main loop, we prefer 
        // to divide by the cosine factor in the eval method.
        return isReflection(wi, wo) ? (*m_albedo)(uv) / std::abs(wi.z) : Black();
    }
    
    float pdf( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return isReflection(wi, wo);
    }

    BsdfSample sample( Sampler& sampler, const Vector& wo, const vec2& uv ) const
    {   
        BsdfSample bs:
        bs.wi = Vector(-wo.x, -wo.y, wo.z);
        bs.pdf = 1;
        bs.weight = (*m_albedo)(uv);
        return bs;
    }
};
```

This models results in flat mirror finish. However, it does not capture the accurate behavior of smooth metals which requires taking into account Fresnel equations, which is further described in the conductors section.

White albedo - Environment lightning  | Gold albedo - Environment lightning 
---|---
![](data/test_bsdf_sphere_mirror.png)|![](data/test_bsdf_sphere_mirror_tint.png)

## 100% transmissive BSDF

The third model is a 100% transmissive material. Although this model is not physically based, it gives (not so) pleasant results but is simple to implement. In addition it is a good introduction to understand the refraction law. This model assumes that the surface is perfectly flat, and the light is refracted in a unique direction through the surface. The direction can be computed using the *Snell-Descartes refraction law*. For further details see Section 8.2.3 in [#Pharr18].   

$$
\omega_i =  \left\{
                \begin{array}{ll}
                    \left( \eta \langle \omega_g, \omega_o \rangle - \cos \theta_i \right) \omega_g - \eta \omega_o \qquad \text{if $\omega_o$ is outside the object}\\
                    \left( \eta \langle \omega_g, \omega_o \rangle + \cos \theta_i \right) \omega_g - \eta \omega_o \qquad \text{if $\omega_o$ is inside the object}
                \end{array}
            \right.
$$

Here $\eta$ is the ratio of refractive index of the media. If $\omega_o$ is outside the object $\eta = \frac{\eta_{int}}{\eta_{ext}}$, else $\eta = \frac{\eta_{ext}}{\eta_{int}}$. If the geometric normal is aligned with the Z axis, $\omega_g = \left(0,0,1\right)$ the refraction simplifies to:

$$
\omega_i =  \left\{
                \begin{array}{ll}
                    \left( - \eta \omega_o.x, - \eta \omega_o.y, - \cos \theta_i \right) \qquad \text{if $\omega_o$ is outside the object}\\
                    \left( - \eta \omega_o.x, - \eta \omega_o.y, + \cos \theta_i \right) \qquad \text{if $\omega_o$ is inside the object}
                \end{array}
            \right.
$$

The angle of refraction $\theta_i$ can be computed using Snell's law and trigonometric identities.

$$ 
\begin{split} 
    & \eta_i \sin \theta_i = \eta_o \sin \theta_o \qquad \text{(Snell's law)} \\
    \iff& \sin \theta_i = \frac{\eta_o}{\eta_i} \sin \theta_o \\
    \iff& \cos \theta_i = \sqrt{1 - \left(\frac{\eta_o}{\eta_i} \sin \theta_o\right)^2} \qquad \text{using} \qquad \cos^2\theta = 1 - \sin^2\theta \\
    \iff& \cos \theta_i = \sqrt{1 - \eta^2 \sin^2 \theta_o} \\
    \iff& \cos \theta_i = \sqrt{1 - \eta^2 (1 - \cos^2 \theta_o)} \qquad \text{using} \qquad \sin^2\theta = 1 - \cos^2\theta
\end{split} 
$$ 
We have now expressed $\theta_i$ function of $\theta_o$, we can implement a small utility function that compute $\theta_i$. N.B. In the unit hemisphere with normal aligned with the Z axis, the cosine term of a direction (inner product with the normal) is the Z component of the vector.
```c++
// compute the refracted cosine term using Snell's law
static float ComputeThetaTransmitted(const float eta, const Vector &wo)
{
    float sinThetaISq = (1.0f - wo.z * wo.z) * eta * eta;                                
    if (sinThetaISq >= 1.0f) return 0; // total internal reflection
    return std::sqrt(std::max(1.0f - sinThetaISq, 0.0f));
}
```

We are finally able to compute refracted directions from the outgoing direction, the normal and the ratio of refractive indices.
```c++
// arbitrary normal vector w
static Vector Refract(const float eta, const Vector &wo, const Vector &w)
{
    float cosThetaI = ComputeThetaTransmitted(eta, wo);
    return (eta * dot(wo, w) - std::copysign(cosThetaI, wo.z)) * w - eta * wo;
}   

// assumes w = vector(0,0,1)
static Vector Refract(const float eta, const Vector &wo)  const
{
    float cosThetaI = ComputeThetaTransmitted(eta, wo);
    return Vector(-wo.x * eta, -wo.y * eta, -std::copysign(cosThetaI, wo.z));
}   
```

![Geometric configuration TODO: remake image](https://upload.wikimedia.org/wikipedia/commons/thumb/f/f0/Refraction_fr.png/300px-Refraction_fr.png)

Similarly to the 100% reflective model, the associated distribution is a Dirac and the pdf of a sampled direction is also 1, since for a given outgoing direction a unique incident direction exists. The **eval** and **pdf** methods can be defined in two ways. A first approach is to return 0 for any direction passed as argument. This follows the principle that a Dirac direction can only be found through the `Bsdf::sample` method and that evaluating the pdf or the function for an incident and outgoing direction does not make sense. A second more flexible approach, consists in returning 1 when the directions $\omega_i$ and $\omega_o$ follow a user-defined constraint of reflection, 0 otherwise (requiring an additional utility function). Finally the complete class writes:
<!-- The BSDF value is always 1, $\rho(\omega_i, \omega_o) = 1$. The associated distribution is a Dirac and the pdf of a sampled direction is also 1, since for a given outgoing direction a unique incident direction exists. The **pdf** method should return 1 when the directions $\omega_i$ and $\omega_o$ follow the constraints of refraction, 0 otherwise. A last utility function is needed here: -->
<!-- For the same reason as for the 100% reflective material we divide by the cosine factor in the **eval** method, $\rho(\omega_i, \omega_o) = \frac{1}{\cos \theta_i}$. Finally the complete class writes: -->
```c++
class BsdfTransmissive : public Bsdf
{
public:
    BsdfTransmissive(Texture* albedo, float intIOR = 1.5f, float extIOR = 1.f) : 
        Bsdf(albedo, SPECULARTRANSMISSION), 
        m_intIOR(intIOR), 
        m_extIOR(extIOR), 
        m_eta(m_extIOR / m_intIOR), 
        m_invEta(1.f / m_eta)  
    {}
    
    Color f( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return Black();
    }
    
    float pdf( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        return 0;
    }

    BsdfSample sample( Sampler& sampler, const Vector& wo, const vec2& uv ) const
    {   
        BsdfSample bs;
        float eta = wo.z < 0.0f ? m_eta : m_invEta;
        bs.wi = Refract(eta, wo);
        bs.pdf = 1;
        bs.weight = (*m_albedo)(uv);
        return bs;
    }

    float m_intIOR, m_extIOR, m_eta, m_invEta;
};

```
As for the 100% reflective model, we can define a more flexible version that tests whether two directions follow a refraction constraint, which writes:
```c++
static bool isRefraction(const Vector& wi, const Vector& wo, float eta, float cosThetaT)
{
    float dotP = -wi.x * wo.x * eta - wi.y * wo.y * eta - std::copysign(cosThetaT, wo.z) * wi.z;
    return std::abs(dotP - 1.0f) < 1e-4;
}

class BsdfTransmissiveFlexible : public Bsdf
{
public:
    BsdfTransmissiveFlexible(std::shared_ptr<Texture> albedo, float intIOR = 1.5f, float extIOR = 1.f) : 
        Bsdf(albedo, SPECULARTRANSMISSION), 
        m_intIOR(intIOR), 
        m_extIOR(extIOR), 
        m_eta(m_extIOR / m_intIOR), 
        m_invEta(1.f / m_eta)  
    {}
    
    Color f( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        if(wi.z == 0 || wo.z == 0) return 0;
        float eta = wo.z < 0.0f ? m_eta : m_invEta;
        float cosThetaI = ComputeThetaTransmitted(eta, wo);
        return isRefraction(wi, wo, eta, cosThetaI) ? (*m_albedo)(uv) / std::abs(wi.z) : Black();
    }
    
    float pdf( const Vector& wi, const Vector& wo, const vec2& uv ) const
    {
        if(wi.z == 0 || wo.z == 0) return 0;
        float eta = wo.z < 0.0f ? m_eta : m_invEta;
        float cosThetaI = ComputeThetaTransmitted(eta, wo);
        return isRefraction(wi, wo, eta, cosThetaI);
    }

    BsdfSample sample( Sampler& sampler, const Vector& wo, const vec2& uv ) const
    {   
        BsdfSample bs;
        float eta = wo.z < 0.0f ? m_eta : m_invEta;
        bs.wi = Refract(eta, wo);
        bs.pdf = 1;
        bs.weight = (*m_albedo)(uv);
        return bs;
    }

    float m_intIOR, m_extIOR, m_eta, m_invEta;
};
```
<!-- This models results in a flat transparent finish. However, it does not capture the accurate behavior of smooth dielectrics which is described in the dielectric section. -->
This models results in flat transparent finish. However, it does not capture the accurate behavior of smooth dielectrics which requires taking into account Fresnel equations, which is further described in the dielectric section.


White albedo - IOR 1.5 | Cyan albedo - IOR 1.5 
---|---
![](data/test_bsdf_sphere_transmissive.png)|![](data/test_bsdf_sphere_transmissive_tint.png)

![Varying the object index of refraction offers a wide range of appearance. The IOR values are from top to bottom and left to right (1.0, 1.02, 1.04, 1.06, 1.08, 1.1, 1.12, 1.14, 1.16, 1.18, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.47).](data/test_bsdf_sphere_transmissive_ior.jpg)

<!-- 
convert +append test_bsdf_sphere_transmissive1.png test_bsdf_sphere_transmissive1.02.png test_bsdf_sphere_transmissive1.04.png test_bsdf_sphere_transmissive1.06.png test_bsdf_sphere_transmissive1.08.png test_bsdf_sphere_transmissive1.1.png test_bsdf_sphere_transmissive1.12.png test_bsdf_sphere_transmissive1.14.png test_bsdf_sphere_transmissive1.16.png test_bsdf_sphere_transmissive1.18.png test_bsdf_sphere_transmissive1-1.2.png 

convert +append test_bsdf_sphere_transmissive1.2.png test_bsdf_sphere_transmissive1.3.png test_bsdf_sphere_transmissive1.4.png test_bsdf_sphere_transmissive1.5.png test_bsdf_sphere_transmissive1.6.png test_bsdf_sphere_transmissive1.7.png test_bsdf_sphere_transmissive1.8.png test_bsdf_sphere_transmissive1.9.png test_bsdf_sphere_transmissive2.png test_bsdf_sphere_transmissive2.42.png test_bsdf_sphere_transmissive1.2-2.42.png 

convert -append test_bsdf_sphere_transmissive1-1.2.png test_bsdf_sphere_transmissive1.2-2.42.png test_bsdf_sphere_transmissive_ior.png

convert -format jpg test_bsdf_sphere_transmissive_ior.png test_bsdf_sphere_transmissive_ior.jpg 
-->

| Material | IOR |
|---|---|
|Air|1.000293 (commonly used value is 1)|
|Water|1.333 |
|Glass|1.52 (commonly used value is 1.5)|
|Diamond|2.417|
[Some values from Wikipedia's [List of refractive indices](https://en.wikipedia.org/wiki/List_of_refractive_indices)]

## Fresnel

In the real world, the amount of transmitted and reflected light light vary with the angle of incidence of the eye direction. Fresnel equations accurately evaluate the reflectance of a material for a given incident angle.
We will not spend much time on Fresnel equations but a short introduction is needed before describing the next models.  
First some references on Fresnel equations :
- The excellent Seb Lagarde's memo on Fresnel equation with some useful references [#Lagarde13] [link](https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/) 
- Naty Hoffman's 2019 MAM talk on Fresnel equations with plenty of details [#Hoffman19] [link](http://renderwonk.com/publications/mam2019/)

### Fresnel reflectance

This is a tough topic and we invite the reader to consult Seb Lagarde's memo and the mentioned sources for further details.
Unlike the reflectance of conductors, the reflectance of dielectric materials is achromatic.

TODO equations

### Schlick approximation

![](data/schlick.png)

### Lazanyi - Schlick approximation

In his 2019 MAM talk, Naty Hoffman propose to use [#Lazanyi05] as a better approximation for the reflectance of materials. 
The idea of [Naty Hoffman's talk](http://renderwonk.com/publications/mam2019/naty_mam2019.pdf) is that Fresnel equations are only approximations, depending how the complex IOR have been computed. Using wrong approximation may lead to worst results than using a tint and Schlick approximation. So going with the approximation is not a bad idea. However its error can be reduced with some modifications in the equation. An additional parameter $h=F(82°)$ is used to modulate the edge tint.  

![](data/fresnel2019.png)

## Conductor

### Conductor Fresnel reflectance (with complex IOR) 

|Copper|Gold|Iron| 
|:---:|:---:|:---:|
|![](data/test_bsdf_sphere_conductor_copper.png)|![](data/test_bsdf_sphere_conductor_gold.png)|![](data/test_bsdf_sphere_conductor_iron.png)|

### Conductor Schlick Fresnel approximation 

Here is the result with Schlick's Fresnel approximation, a white halo appears on the edge and does not match the reference image with the simplified Schlick model. However, it closely matches the reference image at the cost of minor additionnal computation with the Lazanyi - Schlick one.

|Schlick Fresnel approximation|Lazanyi - Schlick Fresnel approximation|Reference|
|---|---|---|
|![Copper](data/test_bsdf_sphere_conductor_tint_copper.png)|![Copper](data/test_bsdf_sphere_conductor_tint_copper_edge_copper.png)|![](data/test_bsdf_sphere_conductor_copper.png)|
|![Difference with the reference](data/test_bsdf_sphere_conductor_tint_copper_difference.png)|![Difference with the reference](data/test_bsdf_sphere_conductor_tint_copper_edge_copper_difference.png)||

### Edge control with the Lazanyi - Schlick approximation 

Another benefit of this model is the ability to smoothly control the edge tint of the material.
|Copper with cyan tinted edges|Gold with purple tinted edges|
|---|---|
|![Copper with cyan tinted edges](data/test_bsdf_sphere_conductor_tint_copper_edge_cyan.png)|![Gold with purple tinted edges](data/bsdf_conductor_gold_tint_purple_edge.png)|

## Dielectric

<!-- ![Smooth dielectric (IOR 1.5)](data/test_bsdf_sphere_dielectric.png) ![Difference with 100% transmissive model](data/test_bsdf_sphere_transmissive_difference.png) -->

Full dielectric model with the Fresnel equations and random selection between reflection and refraction.

![Varying the object index of refraction offers a wide range of appearance. The IOR values are from top to bottom and left to right (1.0, 1.02, 1.04, 1.06, 1.08, 1.1, 1.12, 1.14, 1.16, 1.18, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.47).](data/test_bsdf_sphere_dielectric_ior.jpg)

<!-- 
convert +append test_bsdf_sphere_dielectric1.png test_bsdf_sphere_dielectric1.02.png test_bsdf_sphere_dielectric1.04.png test_bsdf_sphere_dielectric1.06.png test_bsdf_sphere_dielectric1.08.png test_bsdf_sphere_dielectric1.1.png test_bsdf_sphere_dielectric1.12.png test_bsdf_sphere_dielectric1.14.png test_bsdf_sphere_dielectric1.16.png test_bsdf_sphere_dielectric1.18.png test_bsdf_sphere_dielectric1-1.2.png 

convert +append test_bsdf_sphere_dielectric1.2.png test_bsdf_sphere_dielectric1.3.png test_bsdf_sphere_dielectric1.4.png test_bsdf_sphere_dielectric1.5.png test_bsdf_sphere_dielectric1.6.png test_bsdf_sphere_dielectric1.7.png test_bsdf_sphere_dielectric1.8.png test_bsdf_sphere_dielectric1.9.png test_bsdf_sphere_dielectric2.png test_bsdf_sphere_dielectric2.42.png test_bsdf_sphere_dielectric1.2-2.42.png

convert -append test_bsdf_sphere_dielectric1-1.2.png test_bsdf_sphere_dielectric1.2-2.42.png test_bsdf_sphere_dielectric_ior.png

convert -format jpg test_bsdf_sphere_dielectric_ior.png test_bsdf_sphere_dielectric_ior.jpg 
-->

<!-- 
convert +append test_bsdf_sphere_dielectric_nearest1.png test_bsdf_sphere_dielectric_nearest1.02.png test_bsdf_sphere_dielectric_nearest1.04.png test_bsdf_sphere_dielectric_nearest1.06.png test_bsdf_sphere_dielectric_nearest1.08.png test_bsdf_sphere_dielectric_nearest1.1.png test_bsdf_sphere_dielectric_nearest1.12.png test_bsdf_sphere_dielectric_nearest1.14.png test_bsdf_sphere_dielectric_nearest1.16.png test_bsdf_sphere_dielectric_nearest1.18.png test_bsdf_sphere_dielectric_nearest1-1.2.png 

convert +append test_bsdf_sphere_dielectric_nearest1.2.png test_bsdf_sphere_dielectric_nearest1.3.png test_bsdf_sphere_dielectric_nearest1.4.png test_bsdf_sphere_dielectric_nearest1.5.png test_bsdf_sphere_dielectric_nearest1.6.png test_bsdf_sphere_dielectric_nearest1.7.png test_bsdf_sphere_dielectric_nearest1.8.png test_bsdf_sphere_dielectric_nearest1.9.png test_bsdf_sphere_dielectric_nearest2.png test_bsdf_sphere_dielectric_nearest2.42.png test_bsdf_sphere_dielectric_nearest1.2-2.42.png

convert -append test_bsdf_sphere_dielectric_nearest1-1.2.png test_bsdf_sphere_dielectric_nearest1.2-2.42.png test_bsdf_sphere_dielectric_nearest_ior.png

convert -format jpg test_bsdf_sphere_dielectric_nearest_ior.png test_bsdf_sphere_dielectric_nearest_ior.jpg 
-->

## Plastic

<!-- A plastic material is defined by a  -->
- A very comprehensive blog post about BSDF properties [link](https://0xef.wordpress.com/2020/05/01/physically-based-materials-energy-conservation-and-reciprocity/)

Red albedo | Cyan albedo
:---:|:---:
![](data/test_bsdf_sphere_plastic_red.png)|![](data/test_bsdf_sphere_plastic_cyan.png)


## Microfacet model

After smooth surface we would like to model rough / scratched / etched surfaces. Smooth surfaces are assumed perfectly flat at macroscopic and microscopic scale, on the contrary rough materials are flat at macroscopic scale and present perturbations at microscopic scale.  

Flat microsurface | Perturbed microsurface
:---:|:---:
![](data/smooth.png)|![](data/rough.png)
![](data/test_bsdf_sphere_ms_rough_conductor0.png)|![](data/test_bsdf_sphere_ms_rough_conductor1.png)
[Illustrations from Mitsuba 2 documentation TODO make my own schemes]

Microfacet models have been developed to model the distribution of normals on the surface. These models are commonly parameterized by roughness, that controls the amount of perturbations at microscopic scale, and anisotropy that controls the stretch of the distribution w.r.t the surface parameterization. The directional distribution associated with an outgoing direction is a lobe centered around the mirror direction. Several microfacet distributions exist (GGX, Beckmann, Blinn-Phong), however we will only talk about the Throwbridge-Reitz (GGX) distribution, since this is a widely used model in production and is fully analytical. 

![Varying parameters of the GGX distribution. Top row corresponds to an isotropic distribution. Linear roughness ranges horizontally in [0,1] from left to right and anisotropy ranges vertically in [0,1] from top to bottom. The outgoing direction vary over time from a normal angle of incidence ($\theta_o \rightarrow \frac{\pi}{2}$, $\cos \theta_o \rightarrow 0$) to a grazing angle of incidence ($\theta_o \rightarrow 0$, $\cos \theta_o \rightarrow 1$). ](data/test_bsdf_ggx_anisotropic.gif)

### Isotropic

Some common shape invariant distribution of micronormals, based on a statistical heightfield, can be derived, such as GGX / Trowbridge-Reitz, Beckmann, GTR, STD, etc.

Evaluating and sampling is widely studied though very efficient closed form sampling only exists for GGX. We can use some numerical methods to sample the Beckmann distribution very accurately (Jakob 2014). And other models can be sampled using random walk methods (d'Eon 2023, null scattering analogs).

### Anisotropic

Anisotropy comes from the way we stretch the distribution of micronormals (cf: Microsurface transformations).


## Rough diffuse - Oren Nayar

<!-- 
||||||
|---|---|---|---|---|
|![](data/test_bsdf_sphere_oren_nayar0.png)|![](data/test_bsdf_sphere_oren_nayar1.png)|![](data/test_bsdf_sphere_oren_nayar2.png)|![](data/test_bsdf_sphere_oren_nayar3.png)|![](data/test_bsdf_sphere_oren_nayar4.png)|
|![](data/test_bsdf_sphere_oren_nayar5.png)|![](data/test_bsdf_sphere_oren_nayar6.png)|![](data/test_bsdf_sphere_oren_nayar7.png)|![](data/test_bsdf_sphere_oren_nayar8.png)|![](data/test_bsdf_sphere_oren_nayar9.png)| -->

![Oren-Nayar BSDF with varying roughness](data/test_bsdf_sphere_oren_nayar.jpg)


## Anisotropic rough diffuse, conductor and dielectric with GGX

|Rough diffuse (stochastic)|Rough conductor|Rouch dielectric|
|---|---|---|
|![Rough diffuse material with varying roughness and anisotropy](data/test_bsdf_sphere_rough_diffuse_anisotropic_kulla.png)|![Rough conductor with varying roughness and anisotropy](data/test_bsdf_sphere_rough_conductor_anisotropic_kulla.png)|![Rough dielectric with varying roughness and anisotropy](data/test_bsdf_sphere_rough_dielectric_anisotropic_kulla.png)|

## Rough plastic

TODO

## Multiple scattering 

Classic scattering models account only once bounce on the surface, although in the real world we can observe inter-reflections between microfacets. The main idea behind multiple scattering models is to add an energy compensation term that account for the lost energy. The scattering model becomes :

$$
    \rho(\omega_o, \omega_i) = \rho_{ss}(\omega_o, \omega_i) + \rho_{ms}(\omega_o, \omega_i) 
$$

We won't derive existing multiple scattering models here but instead refer to the original articles that are recent and well-explained:
- the approach from Eric Heitz et al. [#Heitz16] which performs a stochastic random walk on the microsurface. 
- the approach from Christopher Kulla and Alejandro Conty [#Kulla17]
- the approach from Emmanuel Turquin [#Turquin19]

<!-- 
Il y a 3 methodes :
1) simuler explicitement le chemin sur la microsurface -> pas de precalculs, code un peu plus consequent, lent a evaluer, vérité terrain
2) ajouter un terme d'energie avec un second lobe diffus : f = fss + fms (fss lobe de base, fms lobe diffus) -> necessite precalcul, assez rapide a evaluer,
3) ajouter un terme d'energie avec un lobe mis a l'echelle equivalent au lobe primaire : f = (1 + k) * fss -> necessite precalcul, le plus simple a mettre en place, mais casse la reciprocité de la bsdf a cause de k 
-->

![Conductor with varying roughness. Top: without multiple scattering compensation. Bottom: with compensation.](data/ms_comparison.jpg)


||Rough diffuse|Rough conductor|Rough dielectric|
|---|---|---|---|
|Single Scattering| ![Anisotropic rough diffuse w/o energy compensation](data/test_bsdf_sphere_rough_diffuse_anisotropic_kulla.png)|![Anisotropic rough conductor w/o energy compensation](data/bsdf_rough_conductor_ggx_copper_ior.png)|![Anisotropic rough dielectric w/o energy compensation](data/bsdf_rough_dielectric_ggx_150.png)|
|Aproximate energy compensation|![Anisotropic rough diffuse with energy compensation](data/test_bsdf_sphere_ms_rough_diffuse_anisotropic_kulla.png)|![Anisotropic rough conductor with energy compensation](data/bsdf_rough_conductor_ggx_copper_ior_multiple_scattering.png)|![Anisotropic rough dielectric with energy compensation](data/bsdf_rough_dielectric_ggx_150_multiple_scattering.png)|

<!-- 
convert +append test_bsdf_sphere_ms_rough_conductor?.png test_bsdf_sphere_ms_rough_conductors.png 
convert +append test_bsdf_sphere_ms_rough_conductor_artistic?.png test_bsdf_sphere_ms_rough_conductors_artistic.png 
convert +append test_bsdf_sphere_rough_conductor?.png test_bsdf_sphere_rough_conductors.png 
convert +append test_bsdf_sphere_rough_conductor_artistic?.png test_bsdf_sphere_rough_conductors_artistic.png 

convert -append test_bsdf_sphere_rough_conductors.png test_bsdf_sphere_ms_rough_conductors.png ms_comparison.png
convert -append test_bsdf_sphere_rough_conductors_artistic.png test_bsdf_sphere_ms_rough_conductors_artistic.png ms_comparison_artistic.png

convert -format jpg ms_comparison.png ms_comparison.jpg
convert -format jpg ms_comparison_artistic.png ms_comparison_artistic.jpg
 -->

<!-- https://therealmjp.github.io/posts/sss-intro/ -->

## Production principled BSDF

> "One model to rule them all" 

TODO

## Subsurface scattering 

TODO

## Phase functions 

TODO

## References

[#Duff17]:      **Building an Orthonormal Basis, Revisited**, Tom Duff, James Burgess, Per Christensen, Christophe Hery, Andrew Kensler, Max Liani, and Ryusuke Villemin, Journal of Computer Graphics Techniques (JCGT), 2017, [link](http://jcgt.org/published/0006/01/01/)

[#Pharr18]:     **Physically based rendering 3rd edition**, Matt Pharr, Wenzel Jakob, and Greg Humphreys, 3rd edition [link](http://www.pbr-book.org/)

[#Heitz14a]:    **Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs**, Eric Heitz, Journal of Computer Graphics Techniques (JCGT), 2014 [link](http://jcgt.org/published/0003/02/03/) 

[#Heitz15]:     **Implementing a Simple Anisotropic Rough Diffuse Material with Stochastic Evaluation**, Eric Heitz and Jonathan Dupuy, Technical report 2015, [link](https://drive.google.com/file/d/0BzvWIdpUpRx_M3ZmakxHYXZWaUk/view)

[#Oren94]:      **Generalization of Lambert’s Reflectance Model**, Michael Oren and Shree K. Nayar, SIGGRAPH, 1994

[#Lazanyi05]:   **Fresnel term approximations for metals**, Lazanyi I. and Szirmay-Kalos L., In The 13th International Conference in Central Europe on Computer Graphics, 2005

<!-- Markdeep: -->
<!-- <link rel="stylesheet" href="https://casual-effects.com/markdeep/latest/slate.css?"> -->
<!-- <link rel="stylesheet" href="style.css">
<style class="fallback">body{visibility:hidden;white-space:pre;font-family:monospace}</style>
<script src="https://casual-effects.com/markdeep/latest/markdeep.min.js"></script>
<script>window.alreadyProcessedMarkdeep||(document.body.style.visibility="visible")</script> -->
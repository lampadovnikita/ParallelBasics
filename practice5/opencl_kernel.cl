__constant float EPSILON = 0.00003f;
__constant float PI = 3.14159265359f;
__constant int SAMPLES = 128;

typedef struct Ray{
	float3 origin;
	float3 dir;
} Ray;

typedef struct Sphere{
	float radius;
	float3 pos;
	float3 color;
	float3 emission;
} Sphere;

/* Получаем псевдослучайное число по двум аргументам */
static float get_random(unsigned int *seed1, unsigned int *seed2) {
	*seed1 = 36969 * ((*seed1) & 65535) + ((*seed1) >> 16);  
	*seed2 = 18000 * ((*seed2) & 65535) + ((*seed2) >> 16);

	unsigned int ires = ((*seed1) << 16) + (*seed2);

	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;
	return (res.f - 2.0f) / 2.0f;
}

/* Создаём вектор из камеры, направленный в заданный пиксель */
Ray createCamRay(const int x_coord, const int y_coord, const int width, const int height){
	
	/* Конвертируем целое число координаты в вещественное число [0, 1] */
	float fx = (float)x_coord / (float)width;
	float fy = (float)y_coord / (float)height;

	float aspect_ratio = (float)(width) / (float)(height);
	float fx2 = (fx - 0.5f) * aspect_ratio;
	float fy2 = fy - 0.5f;

	/* Определяем позицию пикселя на экране */
	float3 pixel_pos = (float3)(fx2, -fy2, 0.0f);

	Ray ray;
	ray.origin = (float3)(0.0f, 0.1f, 2.0f); /* Фиксированная позиция камеры */
	ray.dir = normalize(pixel_pos - ray.origin);

	return ray;
}

/* Расстояние от истока луча до поверхности сферы */
float intersect_sphere(const Sphere* sphere, const Ray* ray)
{
	float3 rayToCenter = sphere->pos - ray->origin;
	float b = dot(rayToCenter, ray->dir);
	float c = dot(rayToCenter, rayToCenter) - sphere->radius*sphere->radius;
	float disc = b * b - c;

	if (disc < 0.0f) return 0.0f;
	else disc = sqrt(disc);

	if ((b - disc) > EPSILON) return b - disc;
	if ((b + disc) > EPSILON) return b + disc;

	return 0.0f;
}

/* Пересекается ли луч с какой-либо сферой на сцене*/
bool intersect_scene(__constant Sphere* spheres, const Ray* ray, float* t, int* sphere_id, const int sphere_count)
{
	float inf = 1e20f;
	*t = inf;

	for (int i = 0; i < sphere_count; i++)  {
		
		Sphere sphere = spheres[i];
		
		float hitdistance = intersect_sphere(&sphere, ray);

		if (hitdistance != 0.0f && hitdistance < *t) {
			*t = hitdistance;
			*sphere_id = i;
		}
	}

	return *t < inf;
}

/* Трассировка пути*/
float3 trace(__constant Sphere* spheres, const Ray* camray, const int sphere_count, const int* seed1, const int* seed2){

	Ray ray = *camray;

	float3 accum_color = (float3)(0.0f, 0.0f, 0.0f);
	float3 mask = (float3)(1.0f, 1.0f, 1.0f);

	for (int bounces = 0; bounces < 8; bounces++){
		/* Расстояние до пересечения */
		float t; 
		/* Индекс сферы с пересечением*/
		int hitsphere_id = 0;

		/* Если луч не столкнулся ни с чем, то возвращаем цвет фона */
		if (!intersect_scene(spheres, &ray, &t, &hitsphere_id, sphere_count))
			return accum_color += mask * (float3)(0.15f, 0.15f, 0.25f);

		Sphere hitsphere = spheres[hitsphere_id];

		/* Вычисляем точку столкновения */
		float3 hitpoint = ray.origin + ray.dir * t;
		
		/* Вычисляем нормаль к поверхности и поворачиваем вектор навстречу лучу */
		float3 normal = normalize(hitpoint - hitsphere.pos); 
		float3 normal_facing = dot(normal, ray.dir) < 0.0f ? normal : normal * (-1.0f);

		/* Случайные числа для нового отскока*/
		float rand1 = 2.0f * PI * get_random(seed1, seed2);
		float rand2 = get_random(seed1, seed2);
		float rand2s = sqrt(rand2);

		float3 w = normal_facing;
		float3 axis = fabs(w.x) > 0.1f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
		float3 u = normalize(cross(axis, w));
		float3 v = cross(w, u);

		/* Вычисляем новое направление */
		float3 newdir = normalize(u * cos(rand1)*rand2s + v*sin(rand1)*rand2s + w*sqrt(1.0f - rand2));

		/* Добавляем небольшой сдвиг, чтобы избежать столкновения с этой же поверхностью */
		ray.origin = hitpoint + normal_facing * EPSILON;
		ray.dir = newdir;

		/* Вычисления цвета */
		accum_color += mask * hitsphere.emission; 

		/* Меняем маску */
		mask *= hitsphere.color; 
		mask *= dot(newdir, normal_facing); 
	}

	return accum_color;
}

__kernel void render_kernel(__constant Sphere* spheres, const int width, const int height, const int sphere_count,
	const unsigned int seed1, const unsigned int seed2, __global float3* output)
{

	unsigned int work_item_id = get_global_id(0);	/* Уникальный глобальный ID для данного пикселя*/

	if (work_item_id >= width * height) {
		return;
	}
	
	unsigned int x_coord = work_item_id % width;
	unsigned int y_coord = work_item_id / width;
	
	/* Для генерации случайных чисел */
	unsigned int saltSeed1 = seed1 + x_coord;
	unsigned int saltSeed2 = seed2 + y_coord;

	Ray camray = createCamRay(x_coord, y_coord, width, height);

	/* Учитываем цвет каждого пути для заданного пикселя*/
	float3 finalcolor = (float3)(0.0f, 0.0f, 0.0f);
	float invSamples = 1.0f / SAMPLES;

	for (int i = 0; i < SAMPLES; i++)
		finalcolor += trace(spheres, &camray, sphere_count, &saltSeed1, &saltSeed2) * invSamples;

	/* Записываем результат цвета в буфер */
	output[work_item_id] = finalcolor;
}

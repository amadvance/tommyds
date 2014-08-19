/*
	Copyright 2014 Gregorius van den Hoven - gregoriusvandenhoven@gmail.com
*/

/*
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define BSC_SCALE 2
#define BSC_LINEAR 2

struct cube
{
	int volume;
	short x_size;
	int *x_floor;
	struct x_node **x_axis;
};

struct x_node
{
	int volume;
	short y_size;
	int *y_floor;
	struct y_node **y_axis;
};

struct y_node
{
	short z_size;
	struct z_node *z_axis;
};

struct z_node
{
	int key;
	void* val;
};

struct z_node *find_index(struct cube *cube, int index, short *x_index, short *y_index, short *z_index);
struct z_node *find_key(struct cube *cube, int key, short *x_index, short *y_index, short *z_index);

struct x_node *split_x_node(struct cube *cube, short x_index);
struct x_node *merge_x_node(struct cube *cube, short x_index1, short x_index2);

void split_y_node(struct cube *cube, short x_index, short y_index);
void merge_y_node(struct cube *cube, short x_index, short y_index1, short y_index2);

void insert_z_node(struct cube *cube, short x_index, short y_index, short z_index, int key, void *val);
void remove_z_node(struct cube *cube, short x_index, short y_index, short z_index);

void inflate_x_axis(struct cube *cube);
void deflate_x_axis(struct cube *cube);

void inflate_y_axis(struct cube *cube, short x_index);
void deflate_y_axis(struct cube *cube, short x_index);

void set_key(struct cube *cube, int key, void *val)
{
	struct z_node *z_node;
	short x_index, y_index, z_index;

	z_node = find_key(cube, key, &x_index, &y_index, &z_index);

	if (z_node)
	{
		z_node->val = val;
	}
	else
	{
		insert_z_node(cube, x_index, y_index, z_index, key, val);
	}
}

struct z_node *get_key(struct cube *cube, int key)
{
	short x_index, y_index, z_index;

	return find_key(cube, key, &x_index, &y_index, &z_index);
}

void del_key(struct cube *cube, int key)
{
	short x_index, y_index, z_index;

	if (find_key(cube, key, &x_index, &y_index, &z_index))
	{
		remove_z_node(cube, x_index, y_index, z_index);
	}
}

struct z_node *get_index(struct cube *cube, int index)
{
	short x_index, y_index, z_index;

	return find_index(cube, index, &x_index, &y_index, &z_index);
}

void del_index(struct cube *cube, int index)
{
	short x_index, y_index, z_index;

	if (find_index(cube, index, &x_index, &y_index, &z_index))
	{
		remove_z_node(cube, x_index, y_index, z_index);
	}
}

struct cube *create_cube(void)
{
	struct cube *cube;

	cube = (struct cube *) calloc(1, sizeof(struct cube));

	return cube;
}

void destroy_cube(struct cube *cube)
{
	if (cube->volume)
	{
		struct x_node *x_node;
		struct y_node *y_node;

		register short x, y, z;

		for (x = cube->x_size - 1 ; x >= 0 ; x--)
		{
			x_node = cube->x_axis[x];
			
			for (y = x_node->y_size - 1 ; y >= 0 ; y--)
			{
				y_node = x_node->y_axis[y];
				
				free(y_node->z_axis);
				free(y_node);
			}
			free(x_node->y_floor);
			free(x_node->y_axis);
			free(x_node);
		}
		free(cube->x_floor);
		free(cube->x_axis);
	}
	free(cube);
}

struct z_node *find_index(struct cube *cube, int index, short *x_index, short *y_index, short *z_index)
{
	struct x_node *x_node;
	struct y_node *y_node;
	register short x, y;
	int total;

	if (index <= 0 || index >= cube->volume)
	{
		if (cube->volume && index == 0)
		{
			*x_index = *y_index = *z_index = 0;

			return &cube->x_axis[0]->y_axis[0]->z_axis[0];
		}
		return NULL;
	}

	if (index < cube->volume / 2)
	{
		total = 0;

		for (x = 0 ; x < cube->x_size ; x++)
		{
			x_node = cube->x_axis[x];

			if (total + x_node->volume > index)
			{
				if (index > total + x_node->volume / 2)
				{
					total += x_node->volume;

					goto backward_y;
				}
				forward_y:

				for (y = 0 ; y < x_node->y_size ; y++)
				{
					y_node = x_node->y_axis[y];

					if (total + y_node->z_size > index)
					{
						*x_index = x;
						*y_index = y;
						*z_index = index - total;

						return &y_node->z_axis[index - total];
					}
					total += y_node->z_size;
				}
			}
			total += x_node->volume;
		}
	}
	else
	{
		total = cube->volume;

		for (x = cube->x_size - 1 ; x >= 0 ; x--)
		{
			x_node = cube->x_axis[x];

			if (total - x_node->volume <= index)
			{
				if (index < total - x_node->volume / 2)
				{
					total -= x_node->volume;

					goto forward_y;
				}
				backward_y:

				for (y = x_node->y_size - 1 ; y >= 0 ; y--)
				{
					y_node = x_node->y_axis[y];

					if (total - y_node->z_size <= index)
					{
						*x_index = x;
						*y_index = y;
						*z_index = y_node->z_size - (total - index);

						return &y_node->z_axis[y_node->z_size - (total - index)];
					}
					total -= y_node->z_size;
				}
			}
			total -= x_node->volume;
		}
	}
	return NULL;
}

struct z_node *find_key(struct cube *cube, int key, short *x_index, short *y_index, short *z_index)
{
	struct x_node *x_node;
	struct y_node *y_node;

	register short bot, mid, x, y, z;

	if (cube->volume == 0 || key < cube->x_floor[0])
	{
		*x_index = *y_index = *z_index = 0;

		return NULL;
	}

	bot = 0;
	mid = x = cube->x_size - 1;

	while (1)
	{
		if (key < cube->x_floor[mid])
		{
			x = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot >= x)
		{
			break;
		}
		mid = x - (x - bot) / 2;
	}

	x_node = cube->x_axis[x];

	bot = 0;
	mid = y = x_node->y_size - 1;

	while (1)
	{
		if (key < x_node->y_floor[mid])
		{
			y = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot >= y)
		{
			break;
		}
		mid = y - (y - bot) / 2;
	}

	y_node = x_node->y_axis[y];

	bot = 0;
	mid = z = y_node->z_size - 1;

	while (1)
	{
		if (key < y_node->z_axis[mid].key)
		{
			z = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot >= z)
		{
			break;
		}

		mid = z - (z - bot) / 2;
	}

	if (key == y_node->z_axis[z].key)
	{
		*x_index = x;
		*y_index = y;
		*z_index = z;

		return &y_node->z_axis[z];
	}

	*x_index = x;
	*y_index = y;
	*z_index = z + 1;

	return NULL;
}

void insert_x_node(struct cube *cube, short x_index)
{
	cube->x_size++;

	cube->x_floor = realloc(cube->x_floor, cube->x_size * sizeof(int));
	cube->x_axis = realloc(cube->x_axis, cube->x_size * sizeof(struct x_node *));

	if (x_index + 1 != cube->x_size)
	{
		memmove(&cube->x_floor[x_index + 1], &cube->x_floor[x_index], (cube->x_size - x_index - 1) * sizeof(int));
		memmove(&cube->x_axis[x_index + 1], &cube->x_axis[x_index], (cube->x_size - x_index - 1) * sizeof(struct x_node *));
	}

	cube->x_axis[x_index] = (struct x_node *) calloc(1, sizeof(struct x_node));
}

void remove_x_node(struct cube *cube, short x_index)
{
	cube->x_size--;

	free(cube->x_axis[x_index]->y_floor);
	free(cube->x_axis[x_index]->y_axis);
	free(cube->x_axis[x_index]);

	if (cube->x_size)
	{
		if (cube->x_size != x_index)
		{
			memmove(&cube->x_floor[x_index], &cube->x_floor[x_index + 1], (cube->x_size - x_index) * sizeof(int));
			memmove(&cube->x_axis[x_index], &cube->x_axis[x_index + 1], (cube->x_size - x_index) * sizeof(struct x_node *));
		}

		cube->x_floor = (int *) realloc(cube->x_floor, cube->x_size * sizeof(int));
		cube->x_axis = (struct x_node **) realloc(cube->x_axis, cube->x_size * sizeof(struct x_node *));
	}
	else
	{
		free(cube->x_floor);
		free(cube->x_axis);
	}
}

void insert_y_node(struct cube *cube, short x_index, short y_index)
{
	struct x_node *x_node = cube->x_axis[x_index];

	x_node->y_size++;

	x_node->y_floor = (int *) realloc(x_node->y_floor, x_node->y_size * sizeof(int));
	x_node->y_axis = (struct y_node **) realloc(x_node->y_axis, x_node->y_size * sizeof(struct y_node *));

	if (x_node->y_size != y_index + 1)
	{
		memmove(&x_node->y_floor[y_index + 1], &x_node->y_floor[y_index], (x_node->y_size - y_index - 1) * sizeof(int));
		memmove(&x_node->y_axis[y_index + 1], &x_node->y_axis[y_index], (x_node->y_size - y_index - 1) * sizeof(struct y_node *));
	}

	x_node->y_axis[y_index] = (struct y_node *) calloc(1, sizeof(struct y_node));
}

void remove_y_node(struct cube *cube, short x_index, short y_index)
{
	struct x_node *x_node = cube->x_axis[x_index];

	x_node->y_size--;

	free(x_node->y_axis[y_index]->z_axis);

	free(x_node->y_axis[y_index]);

	if (x_node->y_size)
	{
		if (x_node->y_size != y_index)
		{
			memmove(&x_node->y_floor[y_index], &x_node->y_floor[y_index + 1], (x_node->y_size - y_index ) * sizeof(int));
			memmove(&x_node->y_axis[y_index], &x_node->y_axis[y_index + 1], (x_node->y_size - y_index ) * sizeof(struct y_node *));
		}

		x_node->y_floor = (int *) realloc(x_node->y_floor, x_node->y_size * sizeof(int));
		x_node->y_axis = (struct y_node **) realloc(x_node->y_axis, x_node->y_size * sizeof(struct y_node *));

		if (y_index == 0)
		{
			cube->x_floor[x_index] = x_node->y_floor[0];
		}
	}
	else
	{
		remove_x_node(cube, x_index);
	}
}

void insert_z_node(struct cube *cube, short x_index, short y_index, short z_index, int key, void *val)
{
	struct x_node *x_node;
	struct y_node *y_node;
	struct z_node *z_node;

	if (cube->volume == 0)
	{
		cube->x_floor = (int *) calloc(1, sizeof(int));
		cube->x_axis = (struct x_node **) calloc(1, sizeof(struct x_node *));
		
		x_node = cube->x_axis[0] = (struct x_node *) calloc(1, sizeof(struct x_node));

		x_node->y_floor = (int *) calloc(1, sizeof(int));
		x_node->y_axis = (struct y_node **) calloc(1, sizeof(struct y_node *));

		y_node = x_node->y_axis[0] = (struct y_node *) calloc(1, sizeof(struct y_node));

		cube->x_size = x_node->y_size = 1;
	}
	else
	{
		x_node = cube->x_axis[x_index];
		y_node = x_node->y_axis[y_index];
	}

	cube->volume++;
	x_node->volume++;

	y_node->z_size++;

	y_node->z_axis = (struct z_node *) realloc(y_node->z_axis, y_node->z_size * sizeof(struct z_node));

	if (x_node->y_size)
	{
		if (z_index + 1 != y_node->z_size)
		{
			memmove(&y_node->z_axis[z_index + 1], &y_node->z_axis[z_index], (y_node->z_size - z_index - 1) * sizeof(struct z_node));
		}
	}
	else
	{
		cube->x_size = x_node->y_size = 1;
	}

	z_node = &y_node->z_axis[z_index];

	z_node->key = key;
	z_node->val = val;

	if (z_index == 0)
	{
		cube->x_floor[0] = x_node->y_floor[0] = z_node->key;
	}

	if (y_node->z_size > cube->x_size * BSC_SCALE)
	{
		split_y_node(cube, x_index, y_index);

		if (x_node->y_size > cube->x_size * BSC_SCALE)
		{
			split_x_node(cube, x_index);

			if ((cube->x_size) * (cube->x_size) * (cube->x_size) > cube->volume * BSC_SCALE)
			{
				deflate_x_axis(cube);
			}
		}
	}
}

void remove_z_node(struct cube *cube, short x_index, short y_index, short z_index)
{
	struct x_node *x_node = cube->x_axis[x_index];
	struct y_node *y_node = x_node->y_axis[y_index];

	cube->volume--;

	x_node->volume--;

	y_node->z_size--;

	if (y_node->z_size != z_index)
	{
		memmove(&y_node->z_axis[z_index], &y_node->z_axis[z_index + 1], (y_node->z_size - z_index) * sizeof(struct z_node));
	}
	y_node->z_axis = (struct z_node *) realloc(y_node->z_axis, y_node->z_size * sizeof(struct z_node));

	if (y_node->z_size)
	{
		if (z_index == 0)
		{
			x_node->y_floor[y_index] = y_node->z_axis[z_index].key;

			if (y_index == 0)
			{
				cube->x_floor[x_index] = y_node->z_axis[z_index].key;
			}
		}

		if (y_node->z_size > cube->x_size * BSC_SCALE)
		{
			split_y_node(cube, x_index, y_index);
		}

		if (x_node->y_size > cube->x_size * BSC_SCALE)
		{
			split_x_node(cube, x_index);
		}

		if ((cube->x_size) * (cube->x_size) * (cube->x_size) > cube->volume * BSC_SCALE)
		{
			deflate_x_axis(cube);
		}
	}
	else
	{
		remove_y_node(cube, x_index, y_index);

		if ((cube->x_size + 1) * (cube->x_size + 1) * (cube->x_size + 1) < cube->volume)
		{
			inflate_x_axis(cube);
		}
	}
}

struct x_node *split_x_node(struct cube *cube, short x_index)
{
	short y;
	int volume;

	struct x_node *x_node1, *x_node2;

	insert_x_node(cube, x_index + 1);

	x_node1 = cube->x_axis[x_index];
	x_node2 = cube->x_axis[x_index + 1];

	x_node2->y_size = x_node1->y_size / 2;
	x_node1->y_size = x_node1->y_size - x_node2->y_size;

	x_node2->y_floor = (int *) realloc(x_node2->y_floor, x_node2->y_size * sizeof(int));
	x_node2->y_axis = (struct y_node **) realloc(x_node2->y_axis, x_node2->y_size * sizeof(struct y_node *));

	memcpy(&x_node2->y_floor[0], &x_node1->y_floor[x_node1->y_size], x_node2->y_size * sizeof(int));
	memcpy(&x_node2->y_axis[0], &x_node1->y_axis[x_node1->y_size], x_node2->y_size * sizeof(struct y_node *));

	x_node1->y_floor = (int *) realloc(x_node1->y_floor, x_node1->y_size * sizeof(int));
	x_node1->y_axis = (struct y_node **) realloc(x_node1->y_axis, x_node1->y_size * sizeof(struct y_node *));

	for (y = volume = 0 ; y < x_node1->y_size ; y++)
	{
		volume += x_node1->y_axis[y]->z_size;
	}

	x_node2->volume = x_node1->volume - volume;
	x_node1->volume = volume;

	cube->x_floor[x_index + 1] = x_node2->y_floor[0];

	return x_node1;
}

struct x_node *merge_x_node(struct cube *cube, short x_index1, short x_index2)
{
	struct x_node *x_node1 = cube->x_axis[x_index1];
	struct x_node *x_node2 = cube->x_axis[x_index2];

	x_node1->y_floor = (int *) realloc(x_node1->y_floor, (x_node1->y_size + x_node2->y_size) * sizeof(int));
	x_node1->y_axis = (struct y_node **) realloc(x_node1->y_axis, (x_node1->y_size + x_node2->y_size) * sizeof(struct y_node *));

	memcpy(&x_node1->y_floor[x_node1->y_size], &x_node2->y_floor[0], x_node2->y_size * sizeof(int));
	memcpy(&x_node1->y_axis[x_node1->y_size], &x_node2->y_axis[0], x_node2->y_size * sizeof(struct y_node *));

	x_node1->y_size += x_node2->y_size;

	x_node1->volume += x_node2->volume;

	remove_x_node(cube, x_index2);

	return x_node1;
}

void split_y_node(struct cube *cube, short x_index, short y_index)
{
	struct x_node *x_node = cube->x_axis[x_index];
	struct y_node *y_node1, *y_node2;

	insert_y_node(cube, x_index, y_index + 1);

	y_node1 = x_node->y_axis[y_index];
	y_node2 = x_node->y_axis[y_index + 1];

	y_node2->z_size = y_node1->z_size / 2;
	y_node1->z_size = y_node1->z_size - y_node2->z_size;

	y_node2->z_axis = (struct z_node *) realloc(y_node2->z_axis, y_node2->z_size * sizeof(struct z_node));

	memcpy(&y_node2->z_axis[0], &y_node1->z_axis[y_node1->z_size], y_node2->z_size * sizeof(struct z_node));

	y_node1->z_axis = (struct z_node *) realloc(y_node1->z_axis, y_node1->z_size * sizeof(struct z_node));

	x_node->y_floor[y_index + 1] = y_node2->z_axis[0].key;
}

void merge_y_node(struct cube *cube, short x_index, short y_index1, short y_index2)
{
	struct y_node *y_node1 = cube->x_axis[x_index]->y_axis[y_index1];
	struct y_node *y_node2 = cube->x_axis[x_index]->y_axis[y_index2];

	y_node1->z_axis = (struct z_node *) realloc(y_node1->z_axis, (y_node1->z_size + y_node2->z_size) * sizeof(struct z_node));

	memcpy(&y_node1->z_axis[y_node1->z_size], &y_node2->z_axis[0], y_node2->z_size * sizeof(struct z_node));

	y_node1->z_size = y_node1->z_size + y_node2->z_size;

	remove_y_node(cube, x_index, y_index2);
}

void inflate_x_axis(struct cube *cube)
{
	short x_index, i_index;

	for (x_index = i_index = 0 ; x_index < cube->x_size ; x_index++)
	{
		if (cube->x_axis[x_index]->volume > cube->x_axis[i_index]->volume)
		{
			i_index = x_index;
		}
	}

	if (cube->x_axis[i_index]->volume > (cube->x_size + 1) * (cube->x_size + 1))
	{
		split_x_node(cube, i_index);

		if (cube->x_axis[i_index]->volume / cube->x_axis[i_index]->y_size > cube->x_size * BSC_SCALE)
		{
			inflate_y_axis(cube, i_index);
			inflate_y_axis(cube, i_index + 1);
		}
	}
}

void inflate_y_axis(struct cube *cube, short x_index)
{
	struct x_node *x_node = cube->x_axis[x_index];
	short y_index;

	for (y_index = 0 ; y_index < x_node->y_size ; y_index++)
	{
		if (x_node->y_axis[y_index]->z_size > cube->x_size * BSC_SCALE)
		{
			split_y_node(cube, x_index, y_index);
		}
	}
}

void deflate_x_axis(struct cube *cube)
{
	short x_index, d_index;

	for (x_index = d_index = 0 ; x_index + 1 < cube->x_size ; x_index++)
	{
		if (cube->x_axis[d_index]->volume + cube->x_axis[d_index + 1]->volume > cube->x_axis[x_index]->volume + cube->x_axis[x_index + 1]->volume)
		{
			d_index = x_index;
		}
	}

	if (cube->x_size > (cube->x_axis[d_index]->volume / cube->x_axis[d_index]->y_size) * BSC_SCALE)
	{
		deflate_y_axis(cube, d_index);
	}

	if (cube->x_size > (cube->x_axis[d_index + 1]->volume / cube->x_axis[d_index + 1]->y_size) * BSC_SCALE)
	{
		deflate_y_axis(cube, d_index + 1);
	}

	merge_x_node(cube, d_index, d_index + 1);
}


void deflate_y_axis(struct cube *cube, short x_index)
{
	struct x_node *x_node = cube->x_axis[x_index];
	short y_index;

	for (y_index = x_node->y_size - 2 ; y_index >= 0 ; y_index--)
	{
		if (cube->x_size >= x_node->y_axis[y_index]->z_size + x_node->y_axis[y_index + 1]->z_size)
		{
			merge_y_node(cube, x_index, y_index, y_index + 1);

			y_index--;
		}
	}
}

#if 0
void show_cube(struct cube *cube, short depth)
{
	struct x_node *x_node;
	struct y_node *y_node;
	short x, y, z;

	for (x = 0 ; x < cube->x_size ; x++)
	{
		x_node = cube->x_axis[x];

		if (depth == 1)
		{
			printf("x index [%4d] y size [%4d] volume [%10d]\n", x, x_node->y_size, x_node->volume);

			continue;
		}

		for (y = 0 ; y < x_node->y_size ; y++)
		{
			y_node = x_node->y_axis[y];

			if (depth == 1)
			{
				printf("x index [%4d] y size [%4d] y index [%4d] z size [%4d]\n", x, x_node->y_size, y, y_node->z_size);

				continue;
			}

			for (z = 0 ; z < y_node->z_size ; z++)
			{
				printf("x index [%4d] y index [%4d] z index [%4d] [%d = %s]\n", x, y, z, y_node->z_axis[z].key, y_node->z_axis[z].val);
			}
		}
	}
}

long long utime()
{
	struct timeval now_time;

	gettimeofday(&now_time, NULL);

	return now_time.tv_sec * 1000000LL + now_time.tv_usec;
}

int main(int argc, char **argv)
{
	int cnt, max;
	long long start, end;

	struct cube *cube;

	if (argv[1] && *argv[1])
	{
		printf("%s\n", argv[1]);
	}

	cube = create_cube();

	max = 100000;

	start = utime();

	for (cnt = 1 ; cnt <= max ; cnt++)
	{
		set_key(cube, 0 + rand(), "rnd order");
//		set_key(cube, 0 + cnt, "fwd order");
//		set_key(cube, 0 - cnt, "rev order");
	}
	end = utime();

	show_cube(cube, 1);

	printf("Time to insert %d elements: %f seconds.\n", max, (end - start) / 1000000.0);

	start = utime();

	for (cnt = max ; cube->volume > 0 ; cnt--)
	{
		del_index(cube, cube->volume - 1);
//		del_key(cube, 0 + cnt);
//		del_key(cube, 0 - cnt);

	}

	end = utime();

	printf("Time to remove %d elements: %f seconds.\n", max, (end - start) / 1000000.0);

	show_cube(cube, 1);

	destroy_cube(cube);

	return 0;
}
#endif

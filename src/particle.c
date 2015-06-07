/**
 * @file 	particle.c
 * @brief 	Particle structure and main particle routines.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * 
 * @section 	LICENSE
 * Copyright (c) 2011 Hanno Rein, Shangfei Liu
 *
 * This file is part of rebound.
 *
 * rebound is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rebound is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "particle.h"
#include "main.h"
#include "tree.h"
#ifndef COLLISIONS_NONE
#include "collisions.h"
#endif // COLLISIONS_NONE
#ifdef MPI
#include "communication_mpi.h"
#endif // MPI

struct particle* 	particles = NULL;	

int N 		= 0;	
int Nmax	= 0;	
int N_active 	= -1; 	
int N_megno 	= 0; 	


// Accessor/mutator functions for accessing beyond library
void set_N(int _N) { N=_N; }
void set_Nmax(int _N) { Nmax=_N; }
void set_N_active(int _N) { N_active=_N; }
void set_N_megno(int _N) { N_megno=_N; }
int get_N() { return N; }
int get_Nmax() { return Nmax; }
int get_N_active() { return N_active; }
int get_N_megno() { return N_megno; }

struct particle* get_particles() { return particles; }

#ifdef BOUNDARIES_OPEN
int boundaries_particle_is_in_box(struct particle p);
int particles_warning_is_not_in_box = 0;
#endif // BOUNDARIES_OPEN
#ifdef GRAVITY_GRAPE
extern double gravity_minimum_mass;
#endif // GRAVITY_GRAPE

void particles_add_local(struct particle pt){
#ifdef BOUNDARIES_OPEN
	if (boundaries_particle_is_in_box(pt)==0){
		// Particle has left the box. Do not add.
		if (particles_warning_is_not_in_box == 0){
			particles_warning_is_not_in_box = 1;
			printf("\nWarning: Trying to add particle which is outside box boundaries.\n");
		}
		return;
	}
#endif // BOUNDARIES_OPEN
	while (Nmax<=N){
		Nmax += 128;
		particles = realloc(particles,sizeof(struct particle)*Nmax);
	}
	particles[N] = pt;
	//        printf("# added particle:  x=(%g,%g,%g) v=(%g,%g,%g) a=(%g,%g,%g) m=%g\n",particles[N].x,particles[N].y,particles[N].z,particles[N].vx,particles[N].vy,particles[N].vz,particles[N].ax,particles[N].ay,particles[N].az,particles[N].m);
#ifdef TREE
	tree_add_particle_to_tree(N);
#endif // TREE
	N++;
}

void particles_add_jl(struct particle* pt){
particles_add(*pt);
}

void particles_add(struct particle pt){
        // printf("# adding particle: x=(%g,%g,%g) v=(%g,%g,%g) a=(%g,%g,%g) m=%g\n",pt.x,pt.y,pt.z,pt.vx,pt.vy,pt.vz,pt.ax,pt.ay,pt.az,pt.m);
	if (N_megno){
		printf("\nWarning: Trying to add particle after calling megno_init().\n");
	}
#ifndef COLLISIONS_NONE
	if (pt.r>=collisions_max_r){
		collisions_max2_r = collisions_max_r;
		collisions_max_r = pt.r;
	}else{
		if (pt.r>=collisions_max2_r){
			collisions_max2_r = pt.r;
		}
	}
#endif 	// COLLISIONS_NONE
#ifdef GRAVITY_GRAPE
	if (pt.m<gravity_minimum_mass){
		gravity_minimum_mass = pt.m;
	}
#endif // GRAVITY_GRAPE
#ifdef MPI
	int rootbox = particles_get_rootbox_for_particle(pt);
	int root_n_per_node = root_n/mpi_num;
	int proc_id = rootbox/root_n_per_node;
	if (proc_id != mpi_id && N >= N_active){
		// Add particle to array and send them to proc_id later. 
		communication_mpi_add_particle_to_send_queue(pt,proc_id);
		return;
	}
#endif // MPI
	// Add particle to local partical array.
	particles_add_local(pt);
}

void particles_add_fixed(struct particle pt,int pos){
	// Only works for non-MPI simulations or when the particles does not move to another node.
#ifdef BOUNDARIES_OPEN
	if (boundaries_particle_is_in_box(pt)==0){
		// Particle has left the box. Do not add.
		return;
	}
#endif // BOUNDARIES_OPEN
	particles[pos] = pt;
#ifdef TREE
	tree_add_particle_to_tree(pos);
#endif // TREE
}

#ifdef LIBREBOUND
int particles_get_rootbox_for_particle(struct particle pt){
	return 0;
}
#else // LIBREBOUND
int particles_get_rootbox_for_particle(struct particle pt){
	int i = ((int)floor((pt.x + boxsize_x/2.)/boxsize)+root_nx)%root_nx;
	int j = ((int)floor((pt.y + boxsize_y/2.)/boxsize)+root_ny)%root_ny;
	int k = ((int)floor((pt.z + boxsize_z/2.)/boxsize)+root_nz)%root_nz;
	int index = (k*root_ny+j)*root_nx+i;
	return index;
}
#endif // LIBREBOUND

void particles_remove_all(void){
	N 		= 0;
	Nmax 		= 0;
	N_active 	= -1;
	N_megno 	= 0;
	free(particles);
	particles 	= NULL;
}

/*
    ZEsarUX  ZX Second-Emulator And Released for UniX
    Copyright (C) 2013 Cesar Hernandez Bano

    This file is part of ZEsarUX.

    ZEsarUX is free software: you can redistribute it and/or modify
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cpu.h"
#include "esxdos_handler.h"
#include "operaciones.h"
#include "debug.h"
#include "menu.h"
#include "utils.h"

#if defined(__APPLE__)
	#include <sys/syslimits.h>
#endif


//Al leer directorio se usa esxdos_handler_root_dir y esxdos_handler_cwd
char esxdos_handler_root_dir[PATH_MAX]="";
char esxdos_handler_cwd[PATH_MAX]="";



//usados al leer directorio
//z80_byte esxdos_handler_filinfo_fattrib;
struct dirent *esxdos_handler_dp;
DIR *esxdos_handler_dfd=NULL;

//Solo soporto abrir un directorio a la vez
z80_byte esxdos_handler_folder_handle;

//ultimo directorio leido al listar archivos
char esxdos_handler_last_dir_open[PATH_MAX]=".";

z80_bit esxdos_handler_enabled={0};

void esxdos_handler_copy_hl_to_string(char *buffer_fichero)
{

	int i;

	for (i=0;peek_byte_no_time(reg_hl+i);i++) {
		buffer_fichero[i]=peek_byte_no_time(reg_hl+i);
	}

	buffer_fichero[i]=0;
}

void esxdos_handler_no_error_uncarry(void)
{
	Z80_FLAGS=(Z80_FLAGS & (255-FLAG_C));
}

void esxdos_handler_error_carry(void)
{
	Z80_FLAGS |=FLAG_C;
}

void esxdos_handler_return_call(void)
{
	reg_pc++;
}


//rellena fullpath con ruta completa
//funcion similar a zxpand_fileopen
void esxdos_handler_pre_fileopen(char *nombre_inicial,char *fullpath)
{


	//Si nombre archivo empieza por /, olvidar cwd
	if (nombre_inicial[0]=='/') sprintf (fullpath,"%s%s",esxdos_handler_root_dir,nombre_inicial);

	//TODO: habria que proteger que en el nombre indicado no se use ../.. para ir a ruta raiz inferior a esxdos_handler_root_dir
	else sprintf (fullpath,"%s/%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd,nombre_inicial);


	int existe_archivo=si_existe_archivo(fullpath);



	//Si no existe buscar archivo sin comparar mayusculas/minusculas
	//en otras partes del codigo directamente se busca primero el archivo con util_busca_archivo_nocase,
	//aqui se hace al reves. Se busca normal, y si no se encuentra, se busca nocase


		if (!existe_archivo) {
			printf ("File %s not found. Searching without case sensitive\n",fullpath);
			char encontrado[PATH_MAX];
			char directorio[PATH_MAX];
			util_get_complete_path(esxdos_handler_root_dir,esxdos_handler_cwd,directorio);
			//sprintf (directorio,"%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd);
			if (util_busca_archivo_nocase ((char *)nombre_inicial,directorio,encontrado) ) {
				printf ("Found with name %s\n",encontrado);
				existe_archivo=1;

				//cambiamos el nombre fullpath y el nombre_inicial por el encontrado
				sprintf ((char *)nombre_inicial,"%s",encontrado);

				sprintf (fullpath,"%s/%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd,(char *)nombre_inicial);
				//sprintf (fullpath,"%s/%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd,encontrado);
				printf ("Found file %s searching without case sensitive\n",fullpath);
			}
		}





}


void esxdos_handler_debug_file_flags(z80_byte b)
{
	if (b&ESXDOS_RST8_FA_READ) printf ("FA_READ|");
	if (b&ESXDOS_RST8_FA_WRITE) printf ("FA_WRITE|");
	if (b&ESXDOS_RST8_FA_OPEN_EX) printf ("FA_OPEN_EX|");
	if (b&ESXDOS_RST8_FA_OPEN_AL) printf ("FA_OPEN_AL|");
	if (b&ESXDOS_RST8_FA_CREATE_NEW) printf ("FA_CREATE_NEW|");
	if (b&ESXDOS_RST8_FA_USE_HEADER) printf ("FA_USE_HEADER|");

	printf ("\n");
}

//Temporales para leer 1 solo archivo
FILE *temp_esxdos_last_open_file_handler_unix=NULL;
z80_byte temp_esxdos_last_open_file_handler;

void esxdos_handler_call_f_open(void)
{
	/*
	;                                                                       // Open file. A=drive. HL=Pointer to null-
;                                                                       // terminated string containg path and/or
;                                                                       // filename. B=file access mode. DE=Pointer
;                                                                       // to BASIC header data/buffer to be filled
;                                                                       // with 8 byte PLUS3DOS BASIC header. If you
;                                                                       // open a headerless file, the BASIC type is
;                                                                       // $ff. Only used when specified in B.
;                                                                       // On return without error, A=file handle.
*/

	esxdos_handler_debug_file_flags(reg_b);

	//Modos soportados
	if (reg_b!=ESXDOS_RST8_FA_READ) {
		printf ("Unsupported fopen mode\n");
		esxdos_handler_error_carry();
		esxdos_handler_return_call();
		return;
	}

	char nombre_archivo[PATH_MAX];
	char fullpath[PATH_MAX];
	esxdos_handler_copy_hl_to_string(nombre_archivo);


	esxdos_handler_pre_fileopen(nombre_archivo,fullpath);

	printf ("fullpath file: %s\n",fullpath);


	//Abrir el archivo.
	temp_esxdos_last_open_file_handler_unix=fopen(fullpath,"rb");


	if (temp_esxdos_last_open_file_handler_unix==NULL) {
		esxdos_handler_error_carry();
		printf ("Error from esxdos_handler_call_f_open file: %s\n",fullpath);
	}
	else {
		temp_esxdos_last_open_file_handler=1;

		reg_a=temp_esxdos_last_open_file_handler;
		esxdos_handler_no_error_uncarry();
		printf ("Successfully esxdos_handler_call_f_open file: %s\n",fullpath);
	}

	esxdos_handler_return_call();


}

void esxdos_handler_call_f_read(void)
{
	if (temp_esxdos_last_open_file_handler_unix==NULL) {
		printf ("Error from esxdos_handler_call_f_read\n");
		esxdos_handler_error_carry();
	}
	else {
		/*
		f_read                  equ fsys_base + 5;      // $9d  sbc a,l
;                                                                       // Read BC bytes at HL from file handle A.
;                                                                       // On return BC=number of bytes successfully
;                                                                       // read. File pointer is updated.
*/
		z80_int total_leidos=0;
		z80_int bytes_a_leer=reg_bc;
		int leidos=1;

		while (bytes_a_leer && leidos) {
			z80_byte byte_read;
			leidos=fread(&byte_read,1,1,temp_esxdos_last_open_file_handler_unix);
			if (leidos) {
					poke_byte_no_time(reg_hl+total_leidos,byte_read);
					total_leidos++;
					bytes_a_leer--;
			}
		}

		reg_bc=total_leidos;
		//reg_hl +=total_leidos; //???
		esxdos_handler_no_error_uncarry();

		printf ("Successfully esxdos_handler_call_f_read total bytes read: %d\n",total_leidos);

	}

	esxdos_handler_return_call();
}

void esxdos_handler_call_f_close(void)
{

	if (temp_esxdos_last_open_file_handler_unix==NULL) {
		esxdos_handler_error_carry();
	}
	else {
		fclose(temp_esxdos_last_open_file_handler_unix);
		temp_esxdos_last_open_file_handler_unix=NULL;
		esxdos_handler_no_error_uncarry();
	}

	esxdos_handler_return_call();
}

//tener en cuenta raiz y directorio actual
//si localdir no es NULL, devolver directorio local (quitando esxdos_handler_root_dir)
//funcion igual a zxpand_get_final_directory, solo adaptando nombres de variables zxpand->esxdos_handler
void esxdos_handler_get_final_directory(char *dir, char *finaldir, char *localdir)
{


	//printf ("esxdos_handler_get_final_directory. dir: %s esxdos_handler_root_dir: %s\n",dir,esxdos_handler_root_dir);

	//Guardamos directorio actual del emulador
	char directorio_actual[PATH_MAX];
	getcwd(directorio_actual,PATH_MAX);

	//cambiar a directorio indicado, juntando raiz, dir actual de esxdos_handler, y dir
	char dir_pedido[PATH_MAX];

	//Si directorio pedido es absoluto, cambiar cwd
	if (dir[0]=='/') {
		sprintf (esxdos_handler_cwd,"%s",dir);
		sprintf (dir_pedido,"%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd);
	}


	else {
		sprintf (dir_pedido,"%s/%s/%s",esxdos_handler_root_dir,esxdos_handler_cwd,dir);
	}

	menu_filesel_chdir(dir_pedido);

	//Ver en que directorio estamos
	//char dir_final[PATH_MAX];
	getcwd(finaldir,PATH_MAX);
	//printf ("total final directory: %s . esxdos_handler_root_dir: %s\n",finaldir,esxdos_handler_root_dir);
	//Esto suele retornar sim barra al final

	//Si finaldir no tiene barra al final, haremos que esxdos_handler_root_dir tampoco la tenga
	int l=strlen(finaldir);

	if (l) {
		if (finaldir[l-1]!='/' && finaldir[l-1]!='\\') {
			//finaldir no tiene barra al final
			int m=strlen(esxdos_handler_root_dir);
			if (m) {
				if (esxdos_handler_root_dir[m-1]=='/' || esxdos_handler_root_dir[m-1]=='\\') {
					//root dir tiene barra al final. quitarla
					//printf ("quitando barra del final de esxdos_handler_root_dir\n");
					esxdos_handler_root_dir[m-1]=0;
				}
			}
		}
	}


	//Ahora hay que quitar la parte del directorio raiz
	//printf ("running strstr (%s,%s)\n",finaldir,esxdos_handler_root_dir);
	char *s=strstr(finaldir,esxdos_handler_root_dir);

	if (s==NULL) {
		debug_printf (VERBOSE_DEBUG,"Directory change not allowed");
		//directorio final es el mismo que habia
		sprintf (finaldir,"%s",esxdos_handler_cwd);
		return;
	}

	//Si esta bien, meter parte local
	if (localdir!=NULL) {
		int l=strlen(esxdos_handler_root_dir);
		sprintf (localdir,"%s",&finaldir[l]);
		//printf ("local directory: %s\n",localdir);
	}

	//printf ("directorio final local de esxdos_handler: %s\n",finaldir);


	//Restauramos directorio actual del emulador
	menu_filesel_chdir(directorio_actual);
}


void esxdos_handler_call_f_chdir(void)
{

	char ruta[PATH_MAX];
	esxdos_handler_copy_hl_to_string(ruta);



		char directorio_final[PATH_MAX];

		printf ("Changing to directory %s\n",ruta);

		esxdos_handler_get_final_directory(ruta,directorio_final,esxdos_handler_cwd);

		printf ("Final directory %s . cwd: %s\n",directorio_final,esxdos_handler_cwd);


	esxdos_handler_no_error_uncarry();
	esxdos_handler_return_call();
}

void esxdos_handler_copy_string_to_hl(char *s)
{
	z80_int p=0;

	while (*s) {
		poke_byte_no_time(reg_hl+p,*s);
		s++;
		p++;
	}

	poke_byte_no_time(reg_hl+p,0);
}

void esxdos_handler_call_f_getcwd(void)
{

	// Get current folder path (null-terminated)
// to buffer. A=drive. HL=pointer to buffer.

	esxdos_handler_copy_string_to_hl(esxdos_handler_cwd);

	esxdos_handler_no_error_uncarry();
	esxdos_handler_return_call();
}

void esxdos_handler_call_f_opendir(void)
{

	/*

	;                                                                       // Open folder. A=drive. HL=Pointer to zero
;                                                                       // terminated string with path to folder.
;                                                                       // B=folder access mode. Only the BASIC
;                                                                       // header bit matters, whether you want to
;                                                                       // read header information or not. On return
;                                                                       // without error, A=folder handle.

*/

	char directorio[PATH_MAX];

	esxdos_handler_copy_hl_to_string(directorio);
	printf ("opening directory %s\n",directorio);

	char directorio_final[PATH_MAX];
	//obtener directorio final
	esxdos_handler_get_final_directory((char *) directorio,directorio_final,NULL);


	//Guardar directorio final, hace falta al leer cada entrada para saber su tamanyo
	sprintf (esxdos_handler_last_dir_open,"%s",directorio_final);
	esxdos_handler_dfd = opendir(directorio_final);

	if (esxdos_handler_dfd == NULL) {
	 	printf("Can't open directory %s (full: %s)\n", directorio,directorio_final);
	  esxdos_handler_error_carry();
	}

	else {
		//Solo soporto abrir un directorio a la vez. Asigno id 1 tal cual
		esxdos_handler_folder_handle=1;
		reg_a=esxdos_handler_folder_handle;
		esxdos_handler_no_error_uncarry();
	}


	esxdos_handler_return_call();
}

//comprobaciones de nombre de archivos en directorio
int esxdos_handler_readdir_no_valido(char *s)
{

	//Si longitud mayor que 12 (8 nombre, punto, 3 extension)
	//if (strlen(s)>12) return 0;

	printf ("checking is name %s is valid\n",s);


	char extension[NAME_MAX];
	char nombre[NAME_MAX];

	util_get_file_extension(s,extension);
	util_get_file_without_extension(s,nombre);

	//si nombre mayor que 8
	if (strlen(nombre)>8) return 0;

	//si extension mayor que 3
	if (strlen(extension)>3) return 0;


	//si hay letras minusculas
	//int i;
	//for (i=0;s[i];i++) {
	//	if (s[i]>='a' && s[i]<'z') return 0;
	//}



	return 1;

}

void esxdos_handler_call_f_readdir(void)
{
	/*
	f_readdir               equ fsys_base + 12;     // $a4  and h
;                                                                       // Read a folder entry to a buffer pointed
;                                                                       // to by HL. A=handle. Buffer format:
;                                                                       // <ASCII>  file/dirname
;                                                                       // <byte>   attributes (MS-DOS format)
;                                                                       // <dword>  date
;                                                                       // <dword>  filesize
;                                                                       // If opened with BASIC header bit, the
;                                                                       // BASIC header follows the normal entry
;                                                                       // (with type=$ff if headerless).
;                                                                       // On return, if A=1 there are more entries.
;                                                                       // If A=0 then it is the end of the folder.
;                                                                       // Does not currently return the size of an
;                                                                       // entry, or zero if end of folder reached.
*/
if (esxdos_handler_dfd==NULL) {
	esxdos_handler_error_carry();
	esxdos_handler_return_call();
	return;
}

do {

	esxdos_handler_dp = readdir(esxdos_handler_dfd);

	if (esxdos_handler_dp == NULL) {
		closedir(esxdos_handler_dfd);
		esxdos_handler_dfd=NULL;
		//no hay mas archivos
		reg_a=0;
		esxdos_handler_no_error_uncarry();
		esxdos_handler_return_call();
		return;
	}


} while(!esxdos_handler_readdir_no_valido(esxdos_handler_dp->d_name));


//if (esxdos_handler_isValidFN(esxdos_handler_globaldata)

//meter flags
//esxdos_handler_filinfo_fattrib=0;



int longitud_nombre=strlen(esxdos_handler_dp->d_name);

//obtener nombre con directorio. obtener combinando directorio root, actual y inicio listado
char nombre_final[PATH_MAX];
util_get_complete_path(esxdos_handler_last_dir_open,esxdos_handler_dp->d_name,nombre_final);
//sprintf (nombre_final,"%s/%s",esxdos_handler_last_dir_open,esxdos_handler_dp->d_name);

//if (get_file_type(esxdos_handler_dp->d_type,esxdos_handler_dp->d_name)==2) {
/*if (get_file_type(esxdos_handler_dp->d_type,nombre_final)==2) {
	//meter flags directorio y nombre entre <>
	//esxdos_handler_filinfo_fattrib |=16;
	sprintf((char *) &esxdos_handler_globaldata[0],"<%s>",esxdos_handler_dp->d_name);
	longitud_nombre +=2;
}

else {
	sprintf((char *) &esxdos_handler_globaldata[0],"%s",esxdos_handler_dp->d_name);
}*/

//Meter nombre
esxdos_handler_copy_string_to_hl(esxdos_handler_dp->d_name);

/*
esxdos_handler_zeddify(&esxdos_handler_globaldata[0]);

//nombre acabado con 0
esxdos_handler_globaldata[longitud_nombre]=0;



int indice=longitud_nombre+1;

//esxdos_handler_globaldata[indice++]=esxdos_handler_filinfo_fattrib;


long int longitud_total=get_file_size(nombre_final);



//copia para ir dividiendo entre 256
long int l=longitud_total;

esxdos_handler_globaldata[indice++]=l&0xFF;

l=l>>8;
esxdos_handler_globaldata[indice++]=l&0xFF;

l=l>>8;
esxdos_handler_globaldata[indice++]=l&0xFF;

l=l>>8;
esxdos_handler_globaldata[indice++]=l&0xFF;

esxdos_handler_latd=0x40;

*/


reg_a=1; //Hay mas ficheros
	esxdos_handler_no_error_uncarry();
	esxdos_handler_return_call();

}

void esxdos_handler_run_normal_rst8(void)
{
	printf ("Running normal rst 8 call\n");
	rst(8);
}

void debug_rst8_esxdos(void)
{
	z80_byte funcion=peek_byte_no_time(reg_pc);

	char buffer_fichero[256];

	switch (funcion)
	{

		case ESXDOS_RST8_DISK_READ:
			printf ("ESXDOS_RST8_DISK_READ\n");
			esxdos_handler_run_normal_rst8();
		break;

		case ESXDOS_RST8_M_GETSETDRV:
			printf ("ESXDOS_RST8_M_GETSETDRV\n");
			esxdos_handler_run_normal_rst8();
	  break;

		case ESXDOS_RST8_F_OPEN:

			esxdos_handler_copy_hl_to_string(buffer_fichero);
			printf ("ESXDOS_RST8_F_OPEN. Mode: %02XH File: %s\n",reg_b,buffer_fichero);
			esxdos_handler_call_f_open();

		break;

		case ESXDOS_RST8_F_CLOSE:
			printf ("ESXDOS_RST8_F_CLOSE\n");
			esxdos_handler_call_f_close();

		break;

		case ESXDOS_RST8_F_READ:
		//Read BC bytes at HL from file handle A.
			printf ("ESXDOS_RST8_F_READ. Read %d bytes at %04XH from file handle %d\n",reg_bc,reg_hl,reg_a);
			esxdos_handler_call_f_read();
		break;

		case ESXDOS_RST8_F_GETCWD:
			printf ("ESXDOS_RST8_F_GETCWD\n");
			esxdos_handler_call_f_getcwd();
		break;

		case ESXDOS_RST8_F_CHDIR:
			esxdos_handler_copy_hl_to_string(buffer_fichero);
			printf ("ESXDOS_RST8_F_CHDIR: %s\n",buffer_fichero);
			esxdos_handler_call_f_chdir();
		break;

		case ESXDOS_RST8_F_OPENDIR:
			printf ("ESXDOS_RST8_F_OPENDIR\n");
			esxdos_handler_call_f_opendir();
		break;

		case ESXDOS_RST8_F_READDIR:
			printf ("ESXDOS_RST8_F_READDIR\n");
			esxdos_handler_call_f_readdir();
		break;



		default:
			if (funcion>=0x80) {
				printf ("Unknown ESXDOS_RST8: %02XH !! \n",funcion);
			}
			rst(8); //No queremos que muestre mensaje de debug
			//esxdos_handler_run_normal_rst8();
		break;
	}
}


void esxdos_handler_run(void)
{
	debug_rst8_esxdos();

}

void esxdos_handler_enable(void)
{
	//root dir se pone directorio actual si esta vacio
if (esxdos_handler_root_dir[0]==0) getcwd(esxdos_handler_root_dir,PATH_MAX);

	esxdos_handler_enabled.v=1;
	//directorio  vacio
	esxdos_handler_cwd[0]=0;
}



void esxdos_handler_disable(void)
{
	esxdos_handler_enabled.v=0;
}

# valerio besozzi 543685
# Si dichiara che il contenuto di questo file e' in ogni sua parte opera
# originale dell'autore

#!/bin/bash

# Controllo se ho due parametri, in caso ne abbia meno di due stampo messaggio
# di errore
if [ $# != 2 ];
then
  echo "Uso: $0 file_di_configurazione tempo" 1>&2
  exit 1
fi

# Controllo se il file passato esista
if ! [ -f $1 ];
then
  echo "Errore: $1 non esiste." 1>&2
  exit 1
fi

# Se mi viene passato --help stampo esempio utilizzo script
if [ "$1" == "--help" ];
then
  echo "Uso: $0 file_di_configurazione tempo" 1>&2
  exit 1
fi

DirName="$(grep -i "DirName" $1 | grep -v "#")"
DirName=${DirName##*"DirName"}
DirName=${DirName#*"="}
DirName=${DirName//[[:space:]]/}
DirName=$DirName/

if [ $2 == 0 ];
then
  echo "La cartella $DirName contiene:"
  echo "$(ls $DirName)"
elif [ $2 > 0 ];
then
  echo "Creo tar con file vecchi di $2"

  Data=$DirName*
  Time="$(date +%s)"

  for D in $Data
  do
    Age="$(stat -c %Y $D)"
    Diff=$(( ($Time - $Age)/60 ))
    if [ $Diff -gt $2 ];
    then
      if ! [ -f backup.tar ];
      then
        tar -cvf  backup.tar --absolute-names $D # Se non esiste tar lo creo
        rm -i -R $D # Rimuovo file
      else
        tar -uvf backup.tar --absolute-names $D # Se tar esiste aggiungo file
        rm -i -R $D # Rimuovo file
      fi
    fi
  done
  gzip backup.tar
fi


exit 0

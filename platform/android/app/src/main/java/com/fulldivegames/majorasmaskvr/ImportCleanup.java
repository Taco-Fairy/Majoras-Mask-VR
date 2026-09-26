package com.fulldivegames.majorasmaskvr;
import java.nio.file.*;
import java.util.Comparator;
import java.util.function.BiConsumer;
/** Best-effort cleanup must never replace an import result or its original error. */
final class ImportCleanup {
 static void run(Path work,Path stage,Path root,BiConsumer<String,Exception> warning) {
  try {
   Path log=work.resolve("extract.log");
   if(Files.isRegularFile(log))Files.copy(log,root.resolve("rom-import.log"),StandardCopyOption.REPLACE_EXISTING);
  }catch(Exception error){warning.accept("preserve extraction log",error);}
  try{Files.deleteIfExists(stage);}catch(Exception error){warning.accept("remove pending archive",error);}
  try(java.util.stream.Stream<Path> paths=Files.walk(work)) {
   for(Path path:(Iterable<Path>)paths.sorted(Comparator.reverseOrder())::iterator) {
    try{Files.deleteIfExists(path);}catch(Exception error){warning.accept("remove temporary import file",error);}
   }
  }catch(Exception error){warning.accept("scan temporary import directory",error);}
 }
}

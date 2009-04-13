/*
* KDevelop C++ Language Support
*
* Copyright 2007 David Nolden <david.nolden.kdevelop@art-master.de>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Library General Public License as
* published by the Free Software Foundation; either version 2 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/** Compatibility:
 * make/automake: Should work perfectly
 * cmake: Thanks to the path-recursion, this works with cmake(tested with version "2.4-patch 6"
 *        with kdelibs out-of-source and with kdevelop4 in-source)
 *
 * unsermake:
 *   unsermake is detected by reading the first line of the makefile. If it contains
 *   "generated by unsermake" the following things are respected:
 *   1. Since unsermake does not have the -W command (which should tell it to recompile
 *      the given file no matter whether it has been changed or not), the file-modification-time of
 *      the file is changed temporarily and the --no-real-compare option is used to force recompilation.
 *   2. The targets seem to be called *.lo instead of *.o when using unsermake, so *.lo names are used.
 *   example-(test)command: unsermake --no-real-compare -n myfile.lo
 **/

#include "includepathresolver.h"

#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <QDir>
#include <QRegExp>

#include <kurl.h>
#include <kprocess.h>
#include <klocale.h>

#ifdef TEST
#include <iostream>
#include <QApplication>
#include <iostream>
using namespace std;
#endif
#include <iostream>

//#define VERBOSE

#if defined(TEST) || defined(VERBOSE)
#define ifTest(x) x
#else
#define ifTest(x)
#endif

///After how many seconds should we retry?
#define CACHE_FAIL_FOR_SECONDS 200

const int processTimeoutSeconds = 40000;

namespace CppTools {
  ///Helper-class used to fake file-modification times
  class FileModificationTimeWrapper {
    public:
      ///@param files list of files that should be fake-modified(modtime will be set to current time)
      FileModificationTimeWrapper( const QStringList& files = QStringList(), const QString& workingDirectory = QString() ) : m_newTime( time(0) )
      {
        QString oldCurrent = QDir::currentPath();

        if( !workingDirectory.isEmpty() )
          QDir::setCurrent( workingDirectory );
        
        for( QStringList::const_iterator it = files.constBegin(); it != files.constEnd(); ++it ) {
          ifTest( cout << "touching " << (*it).toUtf8().constData() << endl );
          struct stat s;
          if( stat( (*it).toLocal8Bit().constData(), &s ) == 0 ) {
            ///Success
            m_stat[*it] = s;
            ///change the modification-time to m_newTime
            struct timeval times[2];
            times[0].tv_sec = m_newTime;
            times[0].tv_usec = 0;
            times[1].tv_sec = m_newTime;
            times[1].tv_usec = 0;

            if( utimes( (*it).toLocal8Bit().constData(), times ) != 0 )
            {
              ifTest( cout << "failed to touch " << (*it).toUtf8().constData() << endl );
            }
          }
        }
        
        if( !workingDirectory.isEmpty() )
          QDir::setCurrent( oldCurrent );
      }

      //Not used yet, might be used to return LD_PRELOAD=.. FAKE_MODIFIED=.. etc. later
      QString commandPrefix() const {
        return QString();
      }

      ///Undo changed modification-times
      void unModify() {
        for( StatMap::const_iterator it = m_stat.constBegin(); it != m_stat.constEnd(); ++it ) {

          ifTest( cout << "untouching " << it.key().toUtf8().constData() << endl );

          struct stat s;
          if( stat( it.key().toLocal8Bit().constData(), &s ) == 0 ) {
            if( s.st_mtime == m_newTime ) {
              ///Still the modtime that we've set, change it back
              struct timeval times[2];
              times[0].tv_usec = 0;
              times[0].tv_sec = s.st_atime;
              times[1].tv_usec = 0;
              times[1].tv_sec = (*it).st_mtime;
              if( utimes( it.key().toLocal8Bit().constData(), times ) != 0 ) {
                ifTest( cout << "failed to untouch " << it.key().toUtf8().constData() << endl );
              }
            } else {
              ///The file was modified since we changed the modtime
              ifTest( cout << "will not untouch " << it.key().toUtf8().constData() << " because the modification-time has changed" << endl );
            }
          }
        }
      }

      ~FileModificationTimeWrapper() {
        unModify();
      }

  private:
    typedef QMap<QString, struct stat> StatMap;
    StatMap m_stat;
    time_t m_newTime;
  };

  class SourcePathInformation {
    public:
    SourcePathInformation( const QString& path ) : m_path( path ), m_isUnsermake(false), m_shouldTouchFiles(false) {
      m_isUnsermake = isUnsermakePrivate( path );

      ifTest( if( m_isUnsermake ) cout << "unsermake detected" << endl );
    }

    bool isUnsermake() const {
      return m_isUnsermake;
    }

    ///When this is set, the file-modification times are changed no matter whether it is unsermake or make
    void setShouldTouchFiles(bool b) {
      m_shouldTouchFiles = b;
    }

    QString getCommand( const QString& sourceFile, const QString& makeParameters ) const {
      if( isUnsermake() )
        return "unsermake -k --no-real-compare -n " + makeParameters;
      else
        return "make -k --no-print-directory -W \'" + sourceFile + "\' -n " + makeParameters;
    }

    bool hasMakefile() const {
        QFileInfo makeFile( m_path, "Makefile" );
        return makeFile.exists();
    }

    bool shouldTouchFiles() const {
      return isUnsermake() || m_shouldTouchFiles;
    }

    QStringList possibleTargets( const QString& targetBaseName ) const {
      QStringList ret;
      ///@todo open the make-file, and read the target-names from there.
      if( isUnsermake() ) {
        //unsermake breaks if the first given target does not exist, so in worst-case 2 calls are necessary
        ret << targetBaseName + ".lo";
        ret << targetBaseName + ".o";
      } else {
        //It would be nice if both targets could be processed in one call, the problem is the exit-status of make, so for now make has to be called twice.
        ret << targetBaseName + ".o";
        ret << targetBaseName + ".lo";
        //ret << targetBaseName + ".lo " + targetBaseName + ".o";
      }
      ret << targetBaseName + ".ko";
      return ret;
    }

    private:
      bool isUnsermakePrivate( const QString& path ) {
        bool ret = false;
        QFileInfo makeFile( path, "Makefile" );
        QFile f( makeFile.absoluteFilePath() );
        if( f.open( QIODevice::ReadOnly ) ) {
          QString firstLine = f.readLine( 128 );
          if( firstLine.indexOf( "generated by unsermake" ) != -1 ) {
            ret = true;
          }
          f.close();
        }
        return ret;
      }

      QString m_path;
      bool m_isUnsermake;
      bool m_shouldTouchFiles;
  };
}

using namespace CppTools;

IncludePathResolver::Cache IncludePathResolver::m_cache;
QMutex IncludePathResolver::m_cacheMutex;

bool IncludePathResolver::executeCommand( const QString& command, const QString& workingDirectory, QString& result ) const
{
  ifTest( cout << "executing " << command.toUtf8().constData() << endl );
  ifTest( cout << "in " << workingDirectory.toUtf8().constData() << endl );

  KProcess proc;
  proc.setWorkingDirectory(workingDirectory);
  proc.setOutputChannelMode(KProcess::MergedChannels);

  QStringList args(command.split(' '));
  QString prog = args.takeFirst();
  proc.setProgram(prog, args);

  int status = proc.execute(processTimeoutSeconds * 1000);
  result = proc.readAll();

  return status == 0;
}

IncludePathResolver::IncludePathResolver() : m_isResolving(false), m_outOfSource(false)  {
}

///More efficient solution: Only do exactly one call for each directory. During that call, mark all source-files as changed, and make all targets for those files.
PathResolutionResult IncludePathResolver::resolveIncludePath( const QString& file ) {
  QFileInfo fi( file );
  return resolveIncludePath( fi.fileName(), fi.absolutePath() );
}

KUrl IncludePathResolver::mapToBuild(const KUrl& url) {
  QString wd = url.toLocalFile();
  if( m_outOfSource ) {
      if( wd.startsWith( m_source ) && !wd.startsWith(m_build) ) {
        //Move the current working-directory out of source, into the build-system
        wd = m_build + '/' + wd.mid( m_source.length() );
        KUrl u( wd );
        u.cleanPath();
        wd = u.toLocalFile();
      }
  }
  return KUrl(wd);
}

PathResolutionResult IncludePathResolver::resolveIncludePath( const QString& file, const QString& _workingDirectory, int maxStepsUp ) {

  struct Enabler {
    bool& b;
    Enabler( bool& bb ) : b(bb) {
      b = true;
    }
    ~Enabler() {
      b = false;
    }
  };

  if( m_isResolving )
    return PathResolutionResult(false, i18n("Tried include path resolution while another resolution process was still running") );

  //Make the working-directory absolute
  QString workingDirectory = _workingDirectory;
  
  if( KUrl(workingDirectory).isRelative() ) {
    KUrl u( QDir::currentPath() );
    
    if(workingDirectory == ".")
      workingDirectory = QString();
    else if(workingDirectory.startsWith("./"))
      workingDirectory = workingDirectory.mid(2);
    
    if(!workingDirectory.isEmpty())
      u.addPath( workingDirectory );
    workingDirectory = u.toLocalFile();
  } else
    workingDirectory = _workingDirectory;

  ifTest( cout << "working-directory: " <<  workingDirectory.toLocal8Bit().data() << "  file: " << file.toLocal8Bit().data() << std::endl; )
  
  QDir sourceDir( workingDirectory );

  ///If there is a .kdev_include_paths file, use it.
// #ifdef READ_CUSTOM_INCLUDE_PATHS
  QFileInfo customIncludePaths( sourceDir, ".kdev_include_paths" );
  if(customIncludePaths.exists()) {
    QFile f(customIncludePaths.filePath());
    if(f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      PathResolutionResult result(true);
      QByteArray read = f.readAll();
      QList<QByteArray> lines = read.split('\n');
      foreach(QByteArray line, lines)
        if(!line.isEmpty()) {
          QString textLine = QString::fromLocal8Bit(line);
          if(textLine.startsWith("RESOLVE:") && !m_outOfSource) {
            std::cout << "found resolve line: " << textLine.toLocal8Bit().data() << std::endl;
            
            //Resolve using the build- and source-directory specified in the file
            
            int sourceIndex = textLine.indexOf(" SOURCE=");
            if(sourceIndex != -1) {
              std::cout << "found source specification" << std::endl;
              int buildIndex = textLine.indexOf(" BUILD=", sourceIndex);
              if(buildIndex != -1) {
                std::cout << "found build specification" << std::endl;
                int sourceStart = sourceIndex+8;
                QString sourceDir = textLine.mid(sourceStart, buildIndex - sourceStart).trimmed();
                int buildStart = buildIndex + 7;
                QString buildDir = textLine.mid(buildStart, textLine.length()-buildStart).trimmed();
                
                std::cout << "directories: " << sourceDir.toLocal8Bit().data() << " " << buildDir.toLocal8Bit().data() << std::endl;
                
                if(sourceDir != buildDir) {
                  setOutOfSourceBuildSystem(sourceDir, buildDir);
                  
                  QStringList fileParts = file.split("/");
                  
                  //Add all directories that were stepped up to the file again
                  
                  QString useFile = file;
                  QString useWorkingDirectory = workingDirectory;
                  if(fileParts.count() > 1) {
                    useWorkingDirectory += "/" +  QStringList(fileParts.mid(0, fileParts.size()-1)).join("/");
                    useFile = fileParts.last();
                  }
                  
                  std::cout << "starting sub-resolver with " << useFile.toLocal8Bit().data() << " " << useWorkingDirectory.toLocal8Bit().data() << std::endl;
                  
                  PathResolutionResult subResult = resolveIncludePath(useFile, useWorkingDirectory, maxStepsUp + fileParts.count() - 1);
                  result.paths += subResult.paths;
                  if(!subResult) {
                    std::cout << "problem in sub-resolver: " << subResult.errorMessage.toLocal8Bit().data() << std::endl;
                  }
                  
                  resetOutOfSourceBuild();
                }
              }
            }
          }else{
            result.paths << textLine;
          }
        }
      
      f.close();
      return result;
    }
  }
// #endif
  
  QDir dir = QDir( mapToBuild(sourceDir.absolutePath()).toLocalFile() );
  
  QFileInfo makeFile( dir, "Makefile" );
  if( !makeFile.exists() ) {
    if(maxStepsUp > 0) {
//       std::cout << "checking one up" << std::endl;
      //If there is no makefile in this directory, go one up and re-try from there
      QFileInfo fileName(file);
      QString localName = sourceDir.dirName();

      if(sourceDir.cdUp() && !fileName.isAbsolute()) {
        QString checkFor = localName + "/" + file;
//         std::cout << "checking in " << sourceDir.toLocalFile().toUtf8().data() << " for " << checkFor.toUtf8().data() << std::endl;;
        PathResolutionResult oneUp = resolveIncludePath(checkFor, sourceDir.path(), maxStepsUp-1);
        if(oneUp.success)
          return oneUp;
      }
    }
    return PathResolutionResult(false, i18n("Makefile is missing in folder \"%1\"", dir.absolutePath()), i18n("Problem while trying to resolve include paths for %1", file ) );
  }

  Enabler e( m_isResolving );

  QStringList cachedPaths; //If the call doesn't succeed, use the cached not up-to-date version
  QDateTime makeFileModification = makeFile.lastModified();
  Cache::iterator it;
  {
    QMutexLocker l( &m_cacheMutex );
    it = m_cache.find( dir.path() );
    if( it != m_cache.end() ) {
      cachedPaths = (*it).paths;
      if( makeFileModification == (*it).modificationTime ) {
        if( !(*it).failed ) {
          //We have a valid cached result
          PathResolutionResult ret(true);
          ret.paths = (*it).paths;
          return ret;
        } else {
          //We have a cached failed result. We should use that for some time but then try again. Return the failed result if: ( there were too many tries within this folder OR this file was already tried ) AND The last tries have not expired yet
          if( /*((*it).failedFiles.size() > 3 || (*it).failedFiles.find( file ) != (*it).failedFiles.end()) &&*/ (*it).failTime.secsTo( QDateTime::currentDateTime() ) < CACHE_FAIL_FOR_SECONDS ) {
            PathResolutionResult ret(false); //Fake that the result is ok
            ret.errorMessage = i18n("Cached: %1", (*it).errorMessage );
            ret.longErrorMessage = (*it).longErrorMessage;
            ret.paths = (*it).paths;
            return ret;
          } else {
            //Try getting a correct result again
          }
        }
      }
    }
  }

  ///STEP 1: Prepare paths
  QString targetName;
  QFileInfo fi( file );

  QString absoluteFile = file;
  if( KUrl( file ).isRelative() )
    absoluteFile = workingDirectory + '/' + file;
  KUrl u( absoluteFile );
  u.cleanPath();
  absoluteFile = u.toLocalFile();

  int dot;
  if( (dot = file.lastIndexOf( '.' )) == -1 )
    return PathResolutionResult( false, i18n( "Filename %1 seems to be malformed", file ) );

  targetName = file.left( dot );

  QString wd = dir.path();
  if( KUrl( wd ).isRelative() ) {
    wd = QDir::currentPath() + '/' + wd;
    KUrl u( wd );
    u.cleanPath();
    wd = u.toLocalFile();
  }

  wd = mapToBuild(wd).toLocalFile();

  SourcePathInformation source( wd );
  QStringList possibleTargets = source.possibleTargets( targetName );

  source.setShouldTouchFiles(true); //Think about whether this should be always enabled. I've enabled it for now so there's an even bigger chance that everything works.

  ///STEP 3: Try resolving the paths, by using once the absolute and once the relative file-path. Which kind is required differs from setup to setup.

  ///STEP 3.1: Try resolution using the absolute path
  PathResolutionResult res;
  //Try for each possible target
  for( QStringList::const_iterator it = possibleTargets.constBegin(); it != possibleTargets.constEnd(); ++it ) {
    res = resolveIncludePathInternal( absoluteFile, wd, *it, source );
    if( res ) {
      break;
    } else {
      ifTest( cout << "Try for possible target " << (*it).toLocal8Bit().data() << " failed: " << res.longErrorMessage.toLocal8Bit().data() << endl; )
    }
  }
  if( res ) {
    QMutexLocker l( &m_cacheMutex );
    CacheEntry ce;
    ce.errorMessage = res.errorMessage;
    ce.longErrorMessage = res.longErrorMessage;
    ce.modificationTime = makeFileModification;
    ce.paths = res.paths;
    m_cache[dir.path()] = ce;
    return res;
  }

  ///STEP 3.2: Try resolution using the relative path
  QString relativeFile = KUrl::relativePath(wd, absoluteFile);
  for( QStringList::const_iterator it = possibleTargets.constBegin(); it != possibleTargets.constEnd(); ++it ) {
    res = resolveIncludePathInternal( relativeFile, wd, *it, source );
    if( res ) break;
  }

  if( res.paths.isEmpty() )
      res.paths = cachedPaths; //We failed, maybe there is an old cached result, use that.

  {
    QMutexLocker l( &m_cacheMutex );
    if( it == m_cache.end() )
      it = m_cache.insert( dir.path(), CacheEntry() );

    CacheEntry& ce(*it);
    ce.paths = res.paths;
    if( ce.modificationTime < makeFileModification )
      ce.modificationTime = makeFileModification;
    if( !res ) {
      ce.failed = true;
      ce.errorMessage = res.errorMessage;
      ce.longErrorMessage = res.longErrorMessage;
      ce.failTime = QDateTime::currentDateTime();
      ce.failedFiles[file] = true;
    } else {
      ce.failed = false;
      ce.failedFiles.clear();
    }
  }

  return res;
}

PathResolutionResult IncludePathResolver::getFullOutput( const QString& command, const QString& workingDirectory, QString& output ) const {
  if( !executeCommand(command, workingDirectory, output) )
    return PathResolutionResult( false, i18n("Make process failed"), i18n("Output: %1", output ) );
  return PathResolutionResult(true);
}

PathResolutionResult IncludePathResolver::resolveIncludePathInternal( const QString& file, const QString& workingDirectory, const QString& makeParameters, const SourcePathInformation& source ) {

  QString processStdout;

  QStringList touchFiles;
  if( source.shouldTouchFiles() ) {
    touchFiles << file;
  }

  FileModificationTimeWrapper touch( touchFiles, workingDirectory );

  QString fullOutput;
  PathResolutionResult res = getFullOutput( source.getCommand( file, makeParameters ), workingDirectory, fullOutput );
  if( !res )
    return res;

  QString includeParameterRx( "\\s(-I|--include-dir=|-I\\s)" );
  QString quotedRx( "(\\').*(\\')|(\\\").*(\\\")" ); //Matches "hello", 'hello', 'hello"hallo"', etc.
  QString escapedPathRx( "(([^)(\"'\\s]*)(\\\\\\s)?)*" ); //Matches /usr/I\ am \ a\ strange\ path/include

  QRegExp includeRx( QString( "%1(%2|%3)(?=\\s)" ).arg( includeParameterRx ).arg( quotedRx ).arg( escapedPathRx ) );
  includeRx.setMinimal( true );
  includeRx.setCaseSensitivity( Qt::CaseSensitive );
  
  QRegExp newLineRx("\\\\\\n");
  fullOutput.replace( newLineRx, "" );
  ///@todo collect multiple outputs at the same time for performance-reasons
  QString firstLine = fullOutput;
  int lineEnd;
  if( (lineEnd = fullOutput.indexOf('\n')) != -1 )
    firstLine.truncate( lineEnd ); //Only look at the first line of output

  /**
   * There's two possible cases this can currently handle.
   * 1.: gcc is called, with the parameters we are searching for(so we parse the parameters)
   * 2.: A recursive make is called, within another directory(so we follow the recursion and try again) "cd /foo/bar && make -f pi/pa/build.make pi/pa/po.o
   * */

  ///STEP 1: Test if it is a recursive make-call
  if( !fullOutput.contains( includeRx ) ) //Do not search for recursive make-calls if we already have include-paths available. Happens in kernel modules.
  {
    QRegExp makeRx( "\\bmake\\s" );
    int offset = 0;
    while( (offset = makeRx.indexIn( firstLine, offset )) != -1 )
    {
      QString prefix = firstLine.left( offset ).trimmed();
      if( prefix.endsWith( "&&") || prefix.endsWith( ';' ) || prefix.isEmpty() )
      {
        QString newWorkingDirectory = workingDirectory;
        ///Extract the new working-directory
        if( !prefix.isEmpty() ) {
          if( prefix.endsWith( "&&" ) )
            prefix.truncate( prefix.length() - 2 );
          else if( prefix.endsWith( ';' ) )
            prefix.truncate( prefix.length() - 1 );

          ///Now test if what we have as prefix is a simple "cd /foo/bar" call.
          
          //In cases like "cd /media/data/kdedev/4.0/build/kdevelop && cd /media/data/kdedev/4.0/build/kdevelop"
          //We use the second directory. For t hat reason we search for the last index of "cd "
          int cdIndex = prefix.lastIndexOf( "cd ");
          if( cdIndex != -1 ) {
            newWorkingDirectory = prefix.right( prefix.length() - 3 - cdIndex ).trimmed();
            if( KUrl( newWorkingDirectory ).isRelative() )
              newWorkingDirectory = workingDirectory + '/' + newWorkingDirectory;
            KUrl u( newWorkingDirectory );
            u.cleanPath();
            newWorkingDirectory = u.toLocalFile();
          }
        }
        QFileInfo d( newWorkingDirectory );
        if( d.exists() ) {
          ///The recursive working-directory exists.
          QString makeParams = firstLine.mid( offset+5 );
          if( !makeParams.contains( ';' ) && !makeParams.contains( "&&" ) ) {
            ///Looks like valid parameters
            ///Make the file-name absolute, so it can be referenced from any directory
            QString absoluteFile = file;
            if( KUrl( absoluteFile ).isRelative() )
              absoluteFile = workingDirectory +  '/' + file;
            KUrl u( absoluteFile );
            u.cleanPath();
            ///Try once with absolute, and if that fails with relative path of the file
            SourcePathInformation newSource( newWorkingDirectory );
            PathResolutionResult res = resolveIncludePathInternal( u.toLocalFile(), newWorkingDirectory, makeParams, newSource );
            if( res )
              return res;
            return resolveIncludePathInternal( KUrl::relativePath(newWorkingDirectory,u.toLocalFile()), newWorkingDirectory, makeParams , newSource );
          }else{
            return PathResolutionResult( false, i18n("Recursive make call failed"), i18n("The parameter string \"%1\" does not seem to be valid. Output was: %2.", makeParams, fullOutput) );
          }
        } else {
          return PathResolutionResult( false, i18n("Recursive make call failed"), i18n("The directory \"%1\" does not exist. Output was: %2.", newWorkingDirectory, fullOutput) );
        }

      } else {
        return PathResolutionResult( false, i18n("Malformed recursive make call"), i18n("Output was: %1", fullOutput) );
      }

      ++offset;
      if( offset >= firstLine.length() ) break;
    }
  }

  ///STEP 2: Search the output for include-paths

  PathResolutionResult ret( true );
  ret.longErrorMessage = fullOutput;
  ifTest( cout << "full output" << fullOutput.toAscii().data() << endl );

  int offset = 0;
  
  while( (offset = includeRx.indexIn( fullOutput, offset )) != -1 ) {
    offset += 1; ///The previous white space
    int pathOffset = 2;
    if( fullOutput[offset+1] == '-' ) {
      ///Must be --include-dir=, with a length of 14 characters
      pathOffset = 14;
    }
    if( fullOutput.length() <= offset + pathOffset )
      break;

    if( fullOutput[offset+pathOffset].isSpace() )
      pathOffset++;

    int start = offset + pathOffset;
    int end = offset + includeRx.matchedLength();

    QString path = fullOutput.mid( start, end-start ).trimmed();
    if( path.startsWith( '"' ) || ( path.startsWith( '\'') && path.length() > 2 ) ) {
      //probable a quoted path
      if( path.endsWith(path.left(1)) ) {
        //Quotation is ok, remove it
        path = path.mid( 1, path.length() - 2 );
      }
    }
    if( KUrl( path ).isRelative() )
      path = workingDirectory + '/' + path;

    KUrl u( path );
    u.cleanPath();

    ret.paths << u.toLocalFile();

    offset = end-1;
  }

  if(ret.paths.isEmpty())
    return PathResolutionResult( false, i18n("Could not extract include paths from make output"), i18n("Folder: \"%1\"  Command: \"%2\"  Output: \"%3\"", workingDirectory, source.getCommand(file, makeParameters), fullOutput) );
  
  return ret;
}

void IncludePathResolver::resetOutOfSourceBuild() {
  m_outOfSource = false;
}

  void IncludePathResolver::setOutOfSourceBuildSystem( const QString& source, const QString& build ) {
    if(source == build) {
      resetOutOfSourceBuild();
      return;
    }
  m_outOfSource = true;
  m_source = source;
  m_build = build;
}

#ifdef TEST

/** This can be used for testing and debugging the system. To compile it use
 * gcc includepathresolver.cpp -I /usr/share/qt3/include -I /usr/include/kde -I ../../lib/util -DTEST -lkdecore -g -o includepathresolver
 * */

int main(int argc, char **argv) {
  QApplication app(argc,argv);
  IncludePathResolver resolver;
  if( argc < 3 ) {
    cout << "params: 1. file-name, 2. working-directory [3. source-directory 4. build-directory]" << endl;
    return 1;
  }
  if( argc >= 5 ) {
    cout << "mapping" << argv[3] << "->" << argv[4] << endl;
    resolver.setOutOfSourceBuildSystem( argv[3], argv[4] );
  }
  PathResolutionResult res = resolver.resolveIncludePath( argv[1], argv[2] );
  cout << "success:" << res.success << "\n";
  if( !res.success ) {
    cout << "error-message: \n" << res.errorMessage.toLocal8Bit().data() << "\n";
    cout << "long error-message: \n" << res.longErrorMessage.toLocal8Bit().data() << "\n";
  }
  cout << "path: \n" << res.paths.join("\n").toLocal8Bit().data() << "\n";
  return res.success;
}
#endif

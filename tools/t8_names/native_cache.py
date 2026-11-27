"""Publish complete native cache files for independent target processes."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

def publish(temp,destination):
    temp=Path(temp);destination=Path(destination)
    try:
        if destination.exists():return
        try:os.replace(temp,destination)
        except OSError:
            if not destination.exists():raise
    finally:
        if temp.exists():temp.unlink()

def text_once(path,text):
    path=Path(path)
    if path.exists():return
    fd,name=tempfile.mkstemp(prefix='native_',suffix='.tmp',dir=path.parent)
    try:
        with os.fdopen(fd,'w',encoding='ascii',newline='\n') as f:f.write(text)
        publish(name,path)
    finally:
        if Path(name).exists():Path(name).unlink()

def compiled(source,cache,cc,flags=('-O3',),dependencies=()):
    source=Path(source);cache=Path(cache);cache.mkdir(exist_ok=True)
    h=hashlib.sha256(source.read_bytes()+repr((cc,flags)).encode())
    for dependency in dependencies:h.update(Path(dependency).read_bytes())
    exe=cache/(source.stem+'_'+h.hexdigest()[:20]+'.exe')
    if not exe.exists():
        fd,name=tempfile.mkstemp(prefix='native_compile_',suffix='.exe',dir=cache);os.close(fd)
        try:
            subprocess.run([cc,*flags,'-o',name,str(source)],check=True);publish(name,exe)
        finally:
            if Path(name).exists():Path(name).unlink()
    return exe

def run_to_file(command,path):
    path=Path(path)
    if path.exists():return
    fd,name=tempfile.mkstemp(prefix='native_output_',suffix='.tmp',dir=path.parent)
    try:
        with os.fdopen(fd,'w',encoding='ascii') as f:subprocess.run(command,stdout=f,check=True)
        publish(name,path)
    finally:
        if Path(name).exists():Path(name).unlink()

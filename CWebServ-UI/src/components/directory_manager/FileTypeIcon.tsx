import { FileIcon, defaultStyles } from 'react-file-icon';

export default function FileTypeIcon({mimetype}:{mimetype:string}){


    const  pattern = /\w+\/(\w+)/gm
    let ext = undefined;
    let match = mimetype.match(pattern)

    if (match){
        const [full, extension ] = match
        ext = extension
    }


    return <FileIcon extension={ext}  />;
}

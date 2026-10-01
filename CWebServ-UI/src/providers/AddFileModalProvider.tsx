import { createContext, useState } from "react";
import { webroot } from "../utils/RequestUtils";
import AddFileForm from "../components/directory_manager/AddFileForm";

export interface FileModalAPI {
    current_path: string;
    showModal: (current_path: string) => void
    hideModal: () => void

}

export var FileModalContext = createContext<FileModalAPI>({
    current_path: webroot,
    showModal: (current_path: string) => { },
    hideModal: () => { },
})

export function AddFileModalProvider({children}){
    const [path, setPath] = useState(webroot)
    const [show, setShow] = useState(false)

    const showModal = (current_path: string) =>{setShow(true); setPath(current_path)}
    const hideModal = ()=>{setShow(false); setPath(webroot)}


    return <FileModalContext.Provider value={{current_path:path, showModal, hideModal}}>
        <AddFileForm show={show} filePath={path}/>
        {children}
    </FileModalContext.Provider>
}

import { useContext, useEffect } from "react"
import { FileTreeContext } from "../../providers/FileTreeProvider";
import { listDirectory, webroot } from "../../utils/RequestUtils";
import FileElement from "../directory_manager/FileElement";



function DirectoryManager({ username }: { username: string }) {
    const { addFile, deleteFile, file_tree, addFiles } = useContext(FileTreeContext)

    useEffect(() => {
        async function fetchFiles() {
            let files = await listDirectory(webroot, "")
            addFiles(webroot, files)
        }
        fetchFiles()
    }, [])

    return <div><p>Hello {username}</p>
        <FileElement path={webroot}/>
    </div>
}
export default DirectoryManager

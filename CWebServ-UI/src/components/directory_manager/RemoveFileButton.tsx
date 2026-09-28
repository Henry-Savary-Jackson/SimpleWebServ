import { useContext } from "react"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Button } from "react-bootstrap"


export default function RemoveFileButton({ filePath }) {
    const { deleteFile } = useContext(FileTreeContext)

    return <Button onClick={(e) => {
        e.stopPropagation()
        deleteFile(filePath)
    }}>X</Button>

}

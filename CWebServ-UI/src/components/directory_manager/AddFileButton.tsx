import { useContext } from "react"
import { FileTreeContext } from "../../providers/FileTreeProvider"
import { Button } from "react-bootstrap"


export default function addFileButton({ filePath }) {
    const { addFile } = useContext(FileTreeContext)

    return <Button onClick={(e) => {
        e.stopPropagation()
    }}>X</Button>

}

import { useContext } from "react"
import { Button } from "react-bootstrap"
import { FileModalContext } from "../../providers/AddFileModalProvider"


export default function addFileButton({path}) {
    const {showModal} = useContext(FileModalContext)

    return <Button variant="Success" onClick={(e) => {
        showModal(path)
        e.stopPropagation()
    }}>+</Button>

}

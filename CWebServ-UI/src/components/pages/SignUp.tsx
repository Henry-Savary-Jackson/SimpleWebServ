import { Link, useLocation } from "react-router";
import { signUp } from "../../utils/RequestUtils";
import { Button, Form, FormLabel } from "react-bootstrap";
import { useCookies } from "react-cookie"
import { useContext, useState } from "react";
import { CSRFContext } from "../../providers/CSRFProvider";
import { useNavigate } from "react-router-dom";

function SignUp() {
    let [cookies, setCookie, removeCookie] = useCookies()
    let [username, setUsername] = useState("")
    let [password, setPassword] = useState("")
    let [passwordCheck, setPasswordCheck] = useState("")
    let [error, setError] = useState("")
    let csrf = useContext(CSRFContext)
    const navigate = useNavigate()


    return <Form onSubmit={async (e) => {
        e.preventDefault();
        e.stopPropagation()
        if (password != passwordCheck) {
            setError("Passwords do not match")
            return
        }
        try { await signUp(username, password, csrf); }
        catch (e) {
            setError(`Failed to sign up:${e}`)
        }
        setCookie("username", username)
        navigate("/main")
    }} >
        <FormLabel htmlFor="username">Username</FormLabel>
        <Form.Control name="username" id="form-username" value={username} onChange={(e) => { setUsername(e.target.value) }} />
        <FormLabel htmlFor="pwd">Password:</FormLabel>
        <Form.Control name="pwd" type={"password"} id="form-pwd" value={password} onChange={(e) => { setPassword(e.target.value) }} />
        <FormLabel htmlFor="pwd-check">Repeat Password:</FormLabel>
        <Form.Control name={"pwd-check"} type={"password"} id="form-pwd-check" value={passwordCheck} onChange={(e) => { setPasswordCheck(e.target.value) }} />
        {password && passwordCheck != password && <FormLabel >Passwords do not match!</FormLabel>}
        <Button disabled={!password && !passwordCheck && password != passwordCheck} type="submit">Sign Up</Button>
        <Link to={"/login"}>Already have an account? Sign Up!</Link>
    </Form>



}

export default SignUp;
